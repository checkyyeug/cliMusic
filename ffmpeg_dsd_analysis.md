# FFmpeg DSD Format Support Analysis

Based on my research and testing with the current FFmpeg build (version N-122378-gec42a0aec0-20260107), here's a comprehensive overview of FFmpeg's support for DSD formats:

## Current DSD Support Status

### Supported DSD Formats:
1. **DSF (DSD Stream File)** - ✅ Supported as demuxer/format
2. **Raw DSD decoders** - ✅ Available in multiple bit-order variants
3. **DSDIFF/DFF** - ❌ Not directly supported as format

### DSD Decoders Available:
- `dsd_lsbf` - DSD (Direct Stream Digital), least significant bit first
- `dsd_lsbf_planar` - DSD, least significant bit first, planar
- `dsd_msbf` - DSD (Direct Stream Digital), most significant bit first  
- `dsd_msbf_planar` - DSD, most significant bit first, planar

All decoders output floating-point PCM (`fltp`) format, indicating they perform DSD-to-PCM conversion internally.

## DSD-to-PCM Conversion Capabilities

**Yes, FFmpeg can directly decode DSD to PCM.** The DSD decoders automatically convert the 1-bit DSD bitstream to floating-point PCM samples. This conversion process involves:

1. **Sample Rate Conversion**: DSD's high sample rates (2.8224 MHz, 5.6448 MHz, etc.) are converted to much lower PCM rates
2. **Bit Depth Conversion**: 1-bit DSD samples are converted to floating-point PCM
3. **Filtering**: The conversion process applies appropriate filtering as part of the delta-sigma demodulation

## Key Limitations and Issues

### 1. **DSDIFF/DFF Format Support**
- FFmpeg does not natively support DSDIFF (.dff) files as a container format
- Only DSF (.dsf) files are supported for DSD content

### 2. **DST (Direct Stream Transfer) Compression**
- DST is lossless compression for DSD used in SACD ISO files and some DFF files
- FFmpeg lacks DST decoder support, limiting compatibility with compressed DSD content

### 3. **DoP (DSD over PCM) Support**
- While FFmpeg developers discussed DoP encoding capabilities in 2014, current builds don't show DoP encoders
- DoP would allow passing DSD samples embedded in PCM streams to external DACs

### 4. **Sample Rate Change Issues**
From the 2014 FFmpeg developer discussions:
- DSD→PCM conversion changes sample rates significantly (e.g., 2.8224 MHz DSD → 352.8 kHz PCM)
- This creates API challenges for libswresample when handling the rate conversion
- The conversion is not mathematically lossless and involves filtering

### 5. **No Native DSD Sample Format**
- FFmpeg doesn't maintain DSD samples in their native 1-bit format internally
- All DSD content gets converted to PCM immediately upon decoding
- This prevents direct DSD manipulation or bit-perfect DSD output

### 6. **Limited DSD-Specific Processing**
- No specialized DSD processing filters or effects
- Cannot perform DSD-specific operations like volume adjustment in the DSD domain

## Technical Implementation Details

The DSD decoders found in FFmpeg:
- Support both bit orders (LSB-first and MSB-first)
- Provide planar and interleaved output options
- Use floating-point output for maximum precision during conversion
- Include threading support for performance

## Conclusion

FFmpeg provides **basic DSD decoding capability** with automatic DSD-to-PCM conversion, but has significant limitations:

✅ **What works:**
- DSF file format support
- Raw DSD bitstream decoding to PCM
- Multiple bit-order variants
- Automatic sample rate conversion

❌ **What's missing:**
- DSDIFF/DFF format support
- DST decompression
- DoP encoding/output
- Native DSD sample format preservation
- DSD-specific processing capabilities

For comprehensive DSD support including DFF files and DST decompression, users typically need specialized tools like `sacd-extract` or commercial solutions rather than relying solely on FFmpeg.