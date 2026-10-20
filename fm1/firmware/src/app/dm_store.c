/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — kits, voice presets and settings in the SPI NOR, on Felucca's flash hooks (storage_hw.c:
 * st_read / st_erase / st_prog, the erase inside the audio dip).
 *
 * Every object has two copies, A and B; a save writes the copy that is not the current one: erase, the payload from
 * offset 256, then the 32-byte commit record at offset 0 last (magic, type, slot, sequence, length, payload CRC,
 * record CRC). At load the valid copy with the newest sequence wins, so a write cut at any point leaves the older one.
 * A kit copy spans two sectors (7936 bytes of payload).
 *
 * Map (inside Felucca's store windows, fm1_flash.h FL_STORE_OK):
 *   kits 1..16        0x97000 .. 0xD6FFF   (slot k: A at 0x97000 + k * 0x4000, B 0x2000 after it)
 *   voice presets     0xD7000 .. 0xDEFFF   (4 banks of 4 voices, a sector per copy)
 *   settings          0xFC000 / 0xFD000
 *   the autosave      0xE5000 .. 0xE6FFF   (one copy of two sectors: a cut write loses only the autosave) */
#define ST_MAGIC 0x4D4D5244u                   /* "DRMM" */
typedef struct {
    uint32_t magic;
    uint16_t type, slot;
    uint32_t seq, len, crc, rsv[2];
    uint32_t hcrc;
} st_hdr_t;
enum { OBJ_KIT0 = 1, OBJ_VPRE0 = OBJ_KIT0 + 16, OBJ_SETTINGS = OBJ_VPRE0 + 4, OBJ_AUTOSAVE, OBJ_COUNT };
#define DM_NKITS 16u
#define DM_NVPRE 16u
#define ST_PAY_OFF 256u

static uint32_t st_crc32(const void *p, uint32_t n)
{
    static const uint32_t T[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    const uint8_t *b = p;
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *b++;
        c = (c >> 4) ^ T[c & 15u];
        c = (c >> 4) ^ T[c & 15u];
    }
    return ~c;
}

static uint32_t obj_base(uint32_t obj, uint32_t copy)
{
    if (obj >= OBJ_KIT0 && obj < OBJ_KIT0 + DM_NKITS)
        return 0x97000u + (obj - OBJ_KIT0) * 0x4000u + copy * 0x2000u;
    if (obj >= OBJ_VPRE0 && obj < OBJ_VPRE0 + 4u)
        return 0xD7000u + (obj - OBJ_VPRE0) * 0x2000u + copy * 0x1000u;
    if (obj == OBJ_SETTINGS)
        return 0xFC000u + copy * 0x1000u;
    return 0xE5000u;                               /* OBJ_AUTOSAVE: one copy */
}
static uint32_t obj_sectors(uint32_t obj) { return obj < OBJ_VPRE0 || obj == OBJ_AUTOSAVE ? 2u : 1u; }
static uint32_t obj_copies(uint32_t obj) { return obj == OBJ_AUTOSAVE ? 1u : 2u; }
static uint32_t obj_max(uint32_t obj) { return obj_sectors(obj) * 0x1000u - ST_PAY_OFF; }

static uint8_t st_buf[0x2000u - ST_PAY_OFF] __attribute__((aligned(4)));

static int st_head(uint32_t obj, uint32_t copy, st_hdr_t *h)
{
    if (st_read(obj_base(obj, copy), h, sizeof *h))
        return -1;
    if (h->magic != ST_MAGIC || h->type != obj || h->slot != copy || h->len > obj_max(obj) ||
        h->hcrc != st_crc32(h, sizeof *h - 4u))
        return -1;
    return 0;
}
static int st_body(uint32_t obj, uint32_t copy, const st_hdr_t *h)   /* payload -> st_buf */
{
    if (st_read(obj_base(obj, copy) + ST_PAY_OFF, st_buf, h->len) || st_crc32(st_buf, h->len) != h->crc)
        return -1;
    return 0;
}
/* the newest valid copy, its payload left in st_buf; -1: none */
static int st_current(uint32_t obj, st_hdr_t *h)
{
    st_hdr_t a, b;
    int va, vb;
    if (!flash_ok)
        return -1;
    va = st_head(obj, 0, &a) == 0;
    vb = obj_copies(obj) > 1u && st_head(obj, 1, &b) == 0;
    if (vb && (!va || (b.seq != a.seq && b.seq - a.seq < 0x80000000u))) {
        if (st_body(obj, 1, &b) == 0) {
            *h = b;
            return 1;
        }
        vb = 0;
    }
    if (va && st_body(obj, 0, &a) == 0) {
        *h = a;
        return 0;
    }
    if (vb && st_body(obj, 1, &b) == 0) {
        *h = b;
        return 1;
    }
    return -1;
}
static int st_used(uint32_t obj)                   /* a valid copy exists (headers only) */
{
    st_hdr_t h;
    if (!flash_ok)
        return 0;
    return st_head(obj, 0, &h) == 0 || (obj_copies(obj) > 1u && st_head(obj, 1, &h) == 0);
}
/* src (len bytes, in RAM) into the copy that is not current. 0 ok; <0 an error */
static int st_save(uint32_t obj, const void *src, uint32_t len)
{
    st_hdr_t h, cur;
    uint32_t seq = 0, base, off, s;
    int c, to, rc;
    if (!flash_ok)
        return -9;
    if (len > obj_max(obj))
        return -6;
    {   /* the copy to write: from the commit records only (st_buf holds the new payload: no body read here) */
        st_hdr_t a, b;
        int va = st_head(obj, 0, &a) == 0, vb = obj_copies(obj) > 1u && st_head(obj, 1, &b) == 0;
        c = vb && (!va || (b.seq != a.seq && b.seq - a.seq < 0x80000000u)) ? 1 : va ? 0 : -1;
        if (c >= 0)
            seq = c ? b.seq : a.seq;
        (void)cur;
    }
    to = obj_copies(obj) > 1u ? (c == 0 ? 1 : 0) : 0;
    base = obj_base(obj, (uint32_t)to);
    if (src != st_buf)
        for (off = 0; off < len; off++)
            st_buf[off] = ((const uint8_t *)src)[off];
    for (s = 0; s < obj_sectors(obj); s++)
        if ((rc = st_erase(base + s * 0x1000u)) != 0)
            return rc;
    for (off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off;
        if ((rc = st_prog(base + ST_PAY_OFF + off, st_buf + off, n)) != 0)
            return rc;
    }
    h.magic = ST_MAGIC;
    h.type = (uint16_t)obj;
    h.slot = (uint16_t)to;
    h.seq = seq + 1u;
    h.len = len;
    h.crc = st_crc32(st_buf, len);
    h.rsv[0] = h.rsv[1] = 0xFFFFFFFFu;
    h.hcrc = st_crc32(&h, sizeof h - 4u);
    if ((rc = st_prog(base, &h, sizeof h)) != 0)
        return rc;
    {
        st_hdr_t chk;
        if (st_head(obj, (uint32_t)to, &chk) || memcmp(&chk, &h, sizeof h) || st_body(obj, (uint32_t)to, &chk))
            return -7;
    }
    return 0;
}

/* ------------------------------------------------------------ kits --- */
static int kit_save(uint32_t k)
{
    uint32_t n = dm_kit_pack(&dm_kit, st_buf, sizeof st_buf);
    if (!n)
        return -6;                                 /* too big for a slot */
    return st_save(k == 0xFFu ? OBJ_AUTOSAVE : OBJ_KIT0 + k, st_buf, n);
}
static int kit_load_into(uint32_t k, dm_kit_t *dst)
{
    st_hdr_t h;
    if (st_current(k == 0xFFu ? OBJ_AUTOSAVE : OBJ_KIT0 + k, &h) < 0)
        return -1;
    return dm_kit_unpack(dst, st_buf, h.len);
}
static int kit_used(uint32_t k) { return st_used(OBJ_KIT0 + k); }
/* the name of a stored kit (12 bytes after the magic), "" if empty */
static void kit_name(uint32_t k, char *out)
{
    st_hdr_t h;
    uint32_t i;
    out[0] = 0;
    if (!flash_ok || (st_head(OBJ_KIT0 + k, 0, &h) && st_head(OBJ_KIT0 + k, 1, &h)))
        return;
    {
        st_hdr_t a, b;
        int va = st_head(OBJ_KIT0 + k, 0, &a) == 0, vb = st_head(OBJ_KIT0 + k, 1, &b) == 0;
        uint32_t copy = vb && (!va || (b.seq != a.seq && b.seq - a.seq < 0x80000000u)) ? 1u : 0u;
        uint8_t nb[16];
        if (st_read(obj_base(OBJ_KIT0 + k, copy) + ST_PAY_OFF, nb, 16))
            return;
        for (i = 0; i < 11u; i++)
            out[i] = (char)(nb[4 + i] >= 32u && nb[4 + i] < 127u ? nb[4 + i] : 0);
        out[11] = 0;
    }
}

/* ---------------------------------------------------- voice presets --- */
/* a bank of 4 voices: [len u16] x 4, then the voices' dm_voice_pack, one after the other (len 0: empty) */
static uint8_t vp_part[4][944];                 /* 4 x 944 + 8 <= 3840: one sector */
static uint16_t vp_len[4];
static int vbank_read(uint32_t bank)
{
    st_hdr_t h;
    uint32_t i, off = 8u;
    for (i = 0; i < 4u; i++)
        vp_len[i] = 0;
    if (st_current(OBJ_VPRE0 + bank, &h) < 0 || h.len < 8u)
        return -1;
    for (i = 0; i < 4u; i++) {
        uint32_t n = (uint32_t)st_buf[2u * i] | (uint32_t)st_buf[2u * i + 1u] << 8, k;
        if (n > sizeof vp_part[0] || off + n > h.len)
            n = 0;
        for (k = 0; k < n; k++)
            vp_part[i][k] = st_buf[off + k];
        vp_len[i] = (uint16_t)n;
        off += n;
    }
    return 0;
}
static int vpre_save(uint32_t slot, const dm_vkit_t *src)
{
    uint32_t i, off = 8u, n;
    vbank_read(slot / 4u);
    {   /* the sound only: the sequence is not part of a preset (and keeps it small) */
        uint32_t l;
        dm_scratch_voice = *src;
        for (l = 0; l < DM_NLANE; l++)
            dm_lane_clear(&dm_scratch_voice, l);
        for (l = 0; l < 4u; l++)
            dm_scratch_voice.manual[l] = 0;
    }
    n = dm_voice_pack(&dm_scratch_voice, vp_part[slot % 4u], sizeof vp_part[0]);
    if (!n)
        return -6;
    vp_len[slot % 4u] = (uint16_t)n;
    for (i = 0; i < 4u; i++) {
        uint32_t k;
        st_buf[2u * i] = (uint8_t)vp_len[i];
        st_buf[2u * i + 1u] = (uint8_t)(vp_len[i] >> 8);
        for (k = 0; k < vp_len[i]; k++)
            st_buf[off++] = vp_part[i][k];
    }
    return st_save(OBJ_VPRE0 + slot / 4u, st_buf, off);
}
/* the preset's sound into dst: its sequencer, sequence and mix stay; a locked control keeps its value */
static int vpre_load(uint32_t slot, dm_vkit_t *dst)
{
    dm_vkit_t *const sp = &dm_scratch_voice;
    if (vbank_read(slot / 4u) || !vp_len[slot % 4u] || dm_voice_unpack(sp, vp_part[slot % 4u], vp_len[slot % 4u]))
        return -1;
    dm_voice_copy_sound(dst, sp);
    return 0;
}
static int vpre_used(uint32_t slot) { return vbank_read(slot / 4u) == 0 && vp_len[slot % 4u] != 0; }

/* --------------------------------------------------------- settings --- */
#define SET_MAGIC 0x31535244u                       /* "DRS1" */
static struct {
    uint32_t magic;
    uint8_t palette, leds, last_kit, clk, usb_fixed, rec_quant, rsv[2];
} dm_set;
static uint8_t set_saved[sizeof dm_set];          /* the copy in the flash */
static void settings_defaults(void)
{
    dm_set.magic = SET_MAGIC;
    dm_set.palette = 3;                             /* ICE */
    dm_set.leds = 0;
    dm_set.last_kit = 0xFE;                         /* none */
    dm_set.clk = DM_CLK_INT;
    dm_set.usb_fixed = 1;
    dm_set.rec_quant = 1;
}
static void settings_load(void)
{
    st_hdr_t h;
    settings_defaults();
    if (st_current(OBJ_SETTINGS, &h) >= 0 && h.len == sizeof dm_set) {
        uint32_t i;
        for (i = 0; i < sizeof dm_set; i++)
            ((uint8_t *)&dm_set)[i] = st_buf[i];
        if (dm_set.magic != SET_MAGIC)
            settings_defaults();
    }
    memcpy(set_saved, &dm_set, sizeof dm_set);
}
/* the settings change on the panel (the menu, the kit slot); written later, while the machine is stopped and only when
 * they differ from the copy in the flash: an erase stops the audio for ~40 ms */
static uint8_t set_pending;
static void settings_save(void) { set_pending = 1; }
static void settings_poll(void)
{
    if (!set_pending || dm_m.playing)
        return;
    set_pending = 0;
    if (!memcmp(set_saved, &dm_set, sizeof dm_set))
        return;
    if (st_save(OBJ_SETTINGS, &dm_set, sizeof dm_set) == 0)
        memcpy(set_saved, &dm_set, sizeof dm_set);
}
