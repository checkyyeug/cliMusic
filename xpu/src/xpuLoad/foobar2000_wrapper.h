/**
 * @file foobar2000_wrapper.h
 * @brief Minimal foobar2000 SDK wrapper for using foo_input_sacd.dll
 *
 * This file contains minimal definitions of foobar2000 SDK interfaces
 * required to use foo_input_sacd.dll as a standalone DLL.
 */

#ifndef FOOBAR2000_WRAPPER_H
#define FOOBAR2000_WRAPPER_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Don't redefine GUID if Windows SDK already has it
#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} GUID;
#endif

// Use a different namespace for foobar2000 GUIDs
#define DEFINE_FOO_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID foo_##name = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }

// Standard service GUIDs (foo_ prefix to avoid conflicts)
DEFINE_FOO_GUID(service_base_guid, 0x1FBD6FF2, 0xA038, 0x4136, 0x85, 0x1D, 0xB6, 0xD7, 0x4E, 0x70, 0x4F, 0x85);

#ifdef __cplusplus
}
#endif

// Forward declarations
class service_base {
public:
    virtual ~service_base() = default;
    virtual void service_add_ref() = 0;
    virtual void service_release() = 0;
};

class input_decoder : public service_base {
public:
    enum {
        flag_sequential = 1,
        flag_parallel = 2,
        flag_no_looping = 4,
        flag_no_background = 8,
    };

    enum {
        info_guid = 0,
        info_channels,
        info_rate,
        info_bits_per_sample,
        info_bitrate,
        info_file_size,
        info_length,
        info_can_seek,
        info_priority,
        info_decoder_name,
        info_color_schemes,
    };

    virtual ~input_decoder() = default;
    virtual bool initialize(const char* filepath, int flags) = 0;
    virtual bool run(const char* filepath) = 0;
    virtual bool get_info(unsigned int what, void* data) = 0;
};

class service_factory {
public:
    virtual ~service_factory() = default;
    virtual service_base* instantiate() = 0;
    virtual const char* get_name() = 0;
    virtual const GUID& get_guid() = 0;
};

// foobar2000_get_interface function type
typedef void* (*foobar2000_get_interface_func)(GUID*);

#endif // FOOBAR2000_WRAPPER_H
