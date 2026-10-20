#include "tusb.h"
#include "pico/unique_id.h"

tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE, .bcdUSB = 0x0200,
    .bDeviceClass = 0x00, .bDeviceSubClass = 0x00, .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x1209,      /* pid.codes */
    .idProduct = 0x0001,     /* PID di prova pid.codes: chiederne uno definitivo prima di produrre in serie */
    .bcdDevice = 0x0100, .iManufacturer = 1, .iProduct = 2, .iSerialNumber = 3, .bNumConfigurations = 1
};
uint8_t const *tud_descriptor_device_cb(void) { return (uint8_t const *)&desc_device; }

enum { ITF_NUM_MIDI = 0, ITF_NUM_MIDI_STREAMING, ITF_NUM_TOTAL };
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)
#define EPNUM_MIDI_OUT 0x01
#define EPNUM_MIDI_IN  0x81
uint8_t const desc_fs_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 0, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, 64)
};
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) { (void)index; return desc_fs_configuration; }

static char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
static char const *string_desc_arr[] = { (const char[]){0x09, 0x04}, "Drummatic", "Drummatic Control", serial };
static uint16_t _desc_str[33];
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    uint8_t chr_count;
    if (index == 0) { memcpy(&_desc_str[1], string_desc_arr[0], 2); chr_count = 1; }
    else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
        if (index == 3) pico_get_unique_board_id_string(serial, sizeof(serial));
        const char *str = string_desc_arr[index];
        chr_count = (uint8_t)strlen(str); if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++) _desc_str[1 + i] = str[i];
    }
    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}
