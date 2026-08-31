#include "opencgm/svg_converter.h"
#include "opencgm/svg/aps_attribute_interpreter.h"
#include "opencgm/svg/aps_geometry.h"
#include "opencgm/svg/aps_serializer.h"
#include "opencgm/svg/aps_text_decoder.h"
#include "opencgm/svg/arc_geometry.h"
#include "opencgm/svg/cell_array.h"
#include "opencgm/svg/color_resolver.h"
#include "opencgm/svg/document_serializer.h"
#include "opencgm/svg/internal_types.h"
#include "opencgm/svg/pattern_geometry.h"
#include "opencgm/svg/stroke_width.h"
#include "opencgm/svg/svg_utils.h"
#include "opencgm/svg/tile_geometry.h"
#include "opencgm/svg/tile_raster.h"
#include "opencgm/svg/xml_utils.h"
#include "opencgm/commands/graphical_primitive_commands.h"
#include "opencgm/commands/attribute_commands.h"
#include "opencgm/commands/picture_descriptor_commands.h"
#include "opencgm/commands/metafile_descriptor_commands.h"
#include "opencgm/commands/delimiter_commands.h"
#include "opencgm/commands/segment_control_commands.h"
#include "opencgm/commands/application_structure_commands.h"
#include "opencgm/commands/control_commands.h"
#include "opencgm/commands/escape_commands.h"
#include "opencgm/utils/sdr_parser.h"
#include "opencgm/utils/hash_utils.h"
#include "opencgm/utils/string_utils.h"
#include "opencgm/version.h"
#include "opencgm/enums.h"
#include "opencgm/nurbs_approximator.h"
#include "opencgm/xcf_parser.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <functional>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <regex>
#include <tuple>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <iterator>
#include <array>
#include <vector>
#include <optional>
#include <stdexcept>
#include <deque>
#include <limits>
#include <mutex>
#include <memory>
#include <cstring>
#include <utility>

#include "../../third_party/miniz/miniz.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <wrl/client.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace opencgm
{
    using svg::escapeXmlAttribute;
    using svg::escapeXmlText;

    namespace
    {
        template <typename T, ClassCode ExpectedClass, int ExpectedId>
        T *metricsCast(Command *cmd)
        {
#ifndef NDEBUG
            if (cmd == nullptr)
            {
                return nullptr;
            }
            if (cmd->elementClass() != ExpectedClass ||
                (ExpectedId >= 0 && cmd->elementId() != ExpectedId))
            {
                return nullptr;
            }
            return dynamic_cast<T *>(cmd);
#else
            if (cmd == nullptr)
            {
                return nullptr;
            }
            if (cmd->elementClass() != ExpectedClass ||
                (ExpectedId >= 0 && cmd->elementId() != ExpectedId))
            {
                return nullptr;
            }
            return static_cast<T *>(cmd);
#endif
        }

    } // namespace

    // Internal types (GlyphOutlineVertex, GlyphOutline, LoadedFont, etc.)
    // are now defined in opencgm/svg/internal_types.h

    static const char kViewerShimInline[] = R"SVGSHIM(
(function(){
  'use strict';
  function parseViewContext(value){
    if(!value){return null;}
    var parts=value.toString().trim().split(/[\s,]+/);
    var result=[];
    for(var i=0;i<parts.length;i++){
      if(!parts[i]){continue;}
      var num=parseFloat(parts[i]);
      if(isNaN(num)){return null;}
      result.push(num);
    }
    if(result.length!==4){return null;}
    return result;
  }
  function isFiniteNumber(n){return typeof n==='number' && isFinite(n);}
  function cssEscapeIdent(value){
    if(typeof CSS!=='undefined' && CSS.escape){
      return CSS.escape(value);
    }
    var str=String(value);
    if(!str.length){return '_';}
    str=str.replace(/^[^a-zA-Z_]/,'_');
    return str.replace(/[^a-zA-Z0-9_\-]/g,'_');
  }
  var script=(typeof document!=='undefined')?document.currentScript:null;
  var svg=null;
  if(script){
    svg=script.ownerSVGElement;
    if(!svg){
      var parent=script.parentNode;
      while(parent && parent.nodeName && parent.nodeName.toLowerCase()!=='svg'){
        parent=parent.parentNode;
      }
      svg=parent;
    }
  }
  if(!svg && typeof document!=='undefined'){
    var svgs=document.getElementsByTagName('svg');
    svg=svgs.length?svgs[svgs.length-1]:null;
  }
  if(!svg){return;}

  var originalViewBoxAttr=svg.getAttribute('data-cgm-original-viewbox') || svg.getAttribute('viewBox') || '';
  var defaultViewBox=parseViewContext(originalViewBoxAttr) || [];

  function toBoolean(value, defaultValue){
    if(value===undefined || value===null){return defaultValue;}
    var lower=String(value).toLowerCase();
    if(lower==='false' || lower==='0' || lower==='no'){return false;}
    if(lower==='true' || lower==='1' || lower==='yes'){return true;}
    return defaultValue;
  }

  function ensureUserVisibilityAttr(node){
    if(!node){return;}
    if(!node.hasAttribute('data-aps-user-visible')){
      node.setAttribute('data-aps-user-visible', node.getAttribute('data-aps-visible') === 'false' ? 'false' : 'true');
    }
  }

  function getUserVisibility(node){
    if(!node){return true;}
    var attr=node.getAttribute('data-aps-user-visible');
    if(attr===null){attr=node.getAttribute('data-aps-visible');}
    return attr===null || attr.toLowerCase()!=='false';
  }

  function setUserVisibility(node, visible){
    if(!node){return;}
    node.setAttribute('data-aps-user-visible', visible ? 'true':'false');
  }

  function computeEffectiveVisibility(node){
    if(!node){return true;}
    if(!getUserVisibility(node)){return false;}
    var parent=node.parentElement;
    while(parent && parent!==svg){
      if(parent.hasAttribute('data-aps-visible')){
        if(!getUserVisibility(parent)){
          return false;
        }
      }
      parent=parent.parentElement;
    }
    return true;
  }

  function updateRegionDisplay(region){
    if(!region){return;}
    if(!region.hasAttribute('data-aps-region-user-visible')){
      region.setAttribute('data-aps-region-user-visible','true');
    }
    var owner=region.closest('[data-aps-visible]');
    var ownerVisible=!owner || owner.getAttribute('data-aps-effective-visible')!=='false';
    var userVisible=region.getAttribute('data-aps-region-user-visible')!=='false';
    region.style.display=(ownerVisible && userVisible)?'':'none';
  }

  function applyVisibility(node){
    if(!node){return;}
    var effective=computeEffectiveVisibility(node);
    node.style.display=effective?'':'none';
    node.setAttribute('data-aps-effective-visible', effective ? 'true':'false');
    var regions=node.querySelectorAll('[data-aps-region-shape="true"]');
    regions.forEach(updateRegionDisplay);
  }

  function updateSubtreeVisibility(node){
    if(!node){return;}
    applyVisibility(node);
    var descendants=node.querySelectorAll('[data-aps-visible]');
    descendants.forEach(function(child){
      if(child!==node){
        applyVisibility(child);
      }
    });
  }

  function initApsVisibility(){
    var nodes=svg.querySelectorAll('[data-aps-visible]');
    nodes.forEach(ensureUserVisibilityAttr);
    nodes.forEach(applyVisibility);
  }

  function initRegionVisibility(){
    var regions=svg.querySelectorAll('[data-aps-region-shape="true"]');
    regions.forEach(updateRegionDisplay);
  }

  function setViewBox(parts){
    if(!parts || parts.length!==4){return false;}
    if(!isFiniteNumber(parts[0]) || !isFiniteNumber(parts[1]) ||
       !isFiniteNumber(parts[2]) || !isFiniteNumber(parts[3])){
      return false;
    }
    svg.setAttribute('viewBox', parts[0] + ' ' + parts[1] + ' ' + parts[2] + ' ' + parts[3]);
    return true;
  }

  function queryLayer(id){
    var escaped=cssEscapeIdent(id);
    return svg.querySelector('[data-aps-type="layer"][data-aps-id="'+escaped+'"]') ||
           svg.querySelector('#'+escaped+'[data-aps-type="layer"]');
  }

  function setLayerVisibility(id, visible){
    var el=queryLayer(id);
    if(!el){return false;}
    setUserVisibility(el, visible);
    updateSubtreeVisibility(el);
    return true;
  }

  function toggleLayer(id){
    var el=queryLayer(id);
    if(!el){return false;}
    var hidden=!getUserVisibility(el);
    return setLayerVisibility(id, hidden);
  }

  function getRegionNodes(ownerId){
    var matches=[];
    var regions=svg.querySelectorAll('[data-aps-region-owner]');
    regions.forEach(function(region){
      if(region.getAttribute('data-aps-region-owner')===ownerId){
        matches.push(region);
      }
    });
    return matches;
  }

  function setRegionVisibility(id, visible){
    var regions=getRegionNodes(id);
    if(!regions.length){return false;}
    regions.forEach(function(region){
      region.setAttribute('data-aps-region-user-visible', visible ? 'true':'false');
      updateRegionDisplay(region);
    });
    return true;
  }

  function toggleRegion(id){
    var regions=getRegionNodes(id);
    if(!regions.length){return false;}
    var nextState=regions[0].getAttribute('data-aps-region-user-visible')==='false';
    return setRegionVisibility(id, nextState);
  }

  function resetViewBox(){
    if(defaultViewBox && defaultViewBox.length===4){
      setViewBox([defaultViewBox[0], defaultViewBox[1], defaultViewBox[2], defaultViewBox[3]]);
    }
  }

  function fitToViewContext(value){
    var rect=parseViewContext(value);
    if(!rect){return false;}
    return setViewBox(rect);
  }

  function findAnchorTarget(node){
    while(node && node!==svg){
      if(node.tagName && node.tagName.toLowerCase()==='a'){
        if(node.hasAttribute('data-aps-viewcontext') || node.getAttribute('data-aps-embed')==='true'){
          return node;
        }
      }
      node=node.parentNode;
    }
    return null;
  }

  initApsVisibility();
  initRegionVisibility();

  svg.addEventListener('click', function(evt){
    var anchor=findAnchorTarget(evt.target);
    if(!anchor){return;}
    var handled=false;
    var vc=anchor.getAttribute('data-aps-viewcontext');
    if(vc){
      handled=fitToViewContext(vc) || handled;
    }
    if(handled || anchor.getAttribute('data-aps-embed')==='true'){
      evt.preventDefault();
    }
  }, true);

  if(typeof window!=='undefined'){
    if(!window.cgmSvgShim){
      window.cgmSvgShim={};
    }
    window.cgmSvgShim.showLayer=function(id){return setLayerVisibility(id,true);};
    window.cgmSvgShim.hideLayer=function(id){return setLayerVisibility(id,false);};
    window.cgmSvgShim.toggleLayer=toggleLayer;
    window.cgmSvgShim.showRegion=function(id){return setRegionVisibility(id,true);};
    window.cgmSvgShim.hideRegion=function(id){return setRegionVisibility(id,false);};
    window.cgmSvgShim.toggleRegion=toggleRegion;
    window.cgmSvgShim.fitToViewContext=fitToViewContext;
    window.cgmSvgShim.resetViewBox=resetViewBox;
  }
})();
)SVGSHIM";

    using svg::addPoints;
    using svg::angularDistanceCCW;
    using svg::base64Encode;
    using svg::buildPngFromIdat;
    using svg::colorToHexString;
    using svg::colorsEqualRgb;
    using svg::colorsNearlyEqualRgb;
    using svg::conjugateDiametersToEllipse;
    using svg::derivePngFormat;
    using svg::ellipseAnglesFromDeltas;
    using svg::ensurePngHeader;
    using svg::hasPngSignature;
    using svg::kAngleTolerance;
    using svg::scalePoint;
    using svg::splitTextIntoLines;
    using svg::subtractPoints;
    using svg::vectorsNearlyEqual;

    std::vector<uint32_t> SVGConverter::utf8ToCodepoints(const std::string &text)
    {
        std::vector<uint32_t> result;
        size_t i = 0;
        const size_t n = text.size();
        while (i < n)
        {
            uint8_t byte = static_cast<uint8_t>(text[i]);
            uint32_t codepoint = 0;
            size_t extra = 0;

            if ((byte & 0x80u) == 0)
            {
                codepoint = byte;
            }
            else if ((byte & 0xE0u) == 0xC0u && i + 1 < n)
            {
                codepoint = byte & 0x1Fu;
                extra = 1;
            }
            else if ((byte & 0xF0u) == 0xE0u && i + 2 < n)
            {
                codepoint = byte & 0x0Fu;
                extra = 2;
            }
            else if ((byte & 0xF8u) == 0xF0u && i + 3 < n)
            {
                codepoint = byte & 0x07u;
                extra = 3;
            }
            else
            {
                // Invalid start byte; skip
                ++i;
                continue;
            }

            bool valid = true;
            for (size_t j = 1; j <= extra; ++j)
            {
                uint8_t cont = static_cast<uint8_t>(text[i + j]);
                if ((cont & 0xC0u) != 0x80u)
                {
                    valid = false;
                    break;
                }
                codepoint = (codepoint << 6) | (cont & 0x3Fu);
            }

            if (valid)
            {
                result.push_back(codepoint);
                i += extra + 1;
            }
            else
            {
                ++i;
            }
        }
        return result;
    }

#ifdef _WIN32
    using Microsoft::WRL::ComPtr;

    static IWICImagingFactory *getWicFactory()
    {
        static std::once_flag factoryInitFlag;
        static ComPtr<IWICImagingFactory> factory;
        static HRESULT initHr = S_OK;

        std::call_once(factoryInitFlag, []() {
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (hr == RPC_E_CHANGED_MODE)
            {
                hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            }
            if (hr == RPC_E_CHANGED_MODE || hr == S_FALSE)
            {
                hr = S_OK;
            }
            if (SUCCEEDED(hr))
            {
                hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&factory));
            }
            initHr = hr;
        });

        if (FAILED(initHr))
        {
            return nullptr;
        }
        return factory.Get();
    }

    static std::vector<uint8_t> buildCcittTiff(uint32_t width, uint32_t height, const std::vector<uint8_t> &payload)
    {
        const uint16_t entryCount = 8;
        const uint32_t headerSize = 8;
        const uint32_t ifdSize = 2 + entryCount * 12 + 4;
        const uint32_t dataOffset = headerSize + ifdSize;

        std::vector<uint8_t> buffer(dataOffset);

        auto write16 = [&](uint32_t offset, uint16_t value) {
            buffer[offset] = static_cast<uint8_t>(value & 0xFF);
            buffer[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        };

        auto write32 = [&](uint32_t offset, uint32_t value) {
            buffer[offset] = static_cast<uint8_t>(value & 0xFF);
            buffer[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
            buffer[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
            buffer[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
        };

        buffer[0] = 'I';
        buffer[1] = 'I';
        write16(2, 42);
        write32(4, 8);
        write16(8, entryCount);

        uint32_t entryOffset = 10;
        auto writeEntry = [&](uint16_t tag, uint16_t type, uint32_t count, uint32_t value) {
            write16(entryOffset, tag);
            write16(entryOffset + 2, type);
            write32(entryOffset + 4, count);
            write32(entryOffset + 8, value);
            entryOffset += 12;
        };

        constexpr uint16_t TIFF_SHORT = 3;
        constexpr uint16_t TIFF_LONG = 4;

        writeEntry(256, TIFF_LONG, 1, width);
        writeEntry(257, TIFF_LONG, 1, height);
        writeEntry(258, TIFF_SHORT, 1, 1);
        writeEntry(259, TIFF_SHORT, 1, 4);
        writeEntry(262, TIFF_SHORT, 1, 0);
        writeEntry(273, TIFF_LONG, 1, dataOffset);
        writeEntry(278, TIFF_LONG, 1, height);
        writeEntry(279, TIFF_LONG, 1, static_cast<uint32_t>(payload.size()));

        write32(entryOffset, 0);

        buffer.insert(buffer.end(), payload.begin(), payload.end());
        return buffer;
    }

    static std::optional<std::vector<uint8_t>> encodeRgbaToPng(IWICImagingFactory *factory,
                                                               const std::vector<uint8_t> &rgba,
                                                               uint32_t width,
                                                               uint32_t height)
    {
        if (!factory || rgba.empty() || width == 0 || height == 0)
        {
            return std::nullopt;
        }

        const UINT stride = width * 4;
        ComPtr<IWICBitmap> bitmap;
        HRESULT hr = factory->CreateBitmapFromMemory(width,
                                                     height,
                                                     GUID_WICPixelFormat32bppRGBA,
                                                     stride,
                                                     static_cast<UINT>(rgba.size()),
                                                     const_cast<BYTE *>(reinterpret_cast<const BYTE *>(rgba.data())),
                                                     &bitmap);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        ComPtr<IStream> memStream(SHCreateMemStream(nullptr, 0));
        if (!memStream)
        {
            return std::nullopt;
        }

        ComPtr<IWICBitmapEncoder> encoder;
        hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = encoder->Initialize(memStream.Get(), WICBitmapEncoderNoCache);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        ComPtr<IWICBitmapFrameEncode> frame;
        ComPtr<IPropertyBag2> frameProps;
        hr = encoder->CreateNewFrame(&frame, &frameProps);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = frame->Initialize(frameProps.Get());
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = frame->SetSize(width, height);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
        hr = frame->SetPixelFormat(&format);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = frame->WriteSource(bitmap.Get(), nullptr);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = frame->Commit();
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = encoder->Commit();
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        STATSTG stat{};
        if (FAILED(memStream->Stat(&stat, STATFLAG_NONAME)))
        {
            return std::nullopt;
        }

        const auto size = static_cast<size_t>(stat.cbSize.QuadPart);
        std::vector<uint8_t> pngData(size);
        LARGE_INTEGER origin{};
        memStream->Seek(origin, STREAM_SEEK_SET, nullptr);
        ULONG bytesRead = 0;
        if (FAILED(memStream->Read(pngData.data(), static_cast<ULONG>(pngData.size()), &bytesRead)))
        {
            return std::nullopt;
        }
        pngData.resize(bytesRead);
        return pngData;
    }

    /**
     * Encode RGBA pixel data to JPEG using Windows Imaging Component
     * @param factory WIC imaging factory
     * @param rgba RGBA pixel data (packed, 4 bytes per pixel)
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @param quality JPEG quality (1-100)
     * @return Encoded JPEG data, or nullopt on failure
     */
    static std::optional<std::vector<uint8_t>> encodeRgbaToJpeg(IWICImagingFactory *factory,
                                                                 const std::vector<uint8_t> &rgba,
                                                                 uint32_t width,
                                                                 uint32_t height,
                                                                 int quality = 85)
    {
        if (!factory || rgba.empty() || width == 0 || height == 0)
        {
            return std::nullopt;
        }

        const UINT stride = width * 4;
        ComPtr<IWICBitmap> bitmap;
        HRESULT hr = factory->CreateBitmapFromMemory(width,
                                                     height,
                                                     GUID_WICPixelFormat32bppRGBA,
                                                     stride,
                                                     static_cast<UINT>(rgba.size()),
                                                     const_cast<BYTE *>(reinterpret_cast<const BYTE *>(rgba.data())),
                                                     &bitmap);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        // Convert RGBA to RGB (JPEG doesn't support alpha)
        ComPtr<IWICFormatConverter> converter;
        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = converter->Initialize(bitmap.Get(),
                                   GUID_WICPixelFormat24bppBGR,  // JPEG uses BGR
                                   WICBitmapDitherTypeNone,
                                   nullptr,
                                   0.0,
                                   WICBitmapPaletteTypeCustom);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        ComPtr<IStream> memStream(SHCreateMemStream(nullptr, 0));
        if (!memStream)
        {
            return std::nullopt;
        }

        ComPtr<IWICBitmapEncoder> encoder;
        hr = factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = encoder->Initialize(memStream.Get(), WICBitmapEncoderNoCache);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        ComPtr<IWICBitmapFrameEncode> frame;
        ComPtr<IPropertyBag2> frameProps;
        hr = encoder->CreateNewFrame(&frame, &frameProps);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        // Set JPEG quality property
        PROPBAG2 qualityProp = {};
        qualityProp.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT qualityValue;
        VariantInit(&qualityValue);
        qualityValue.vt = VT_R4;
        qualityValue.fltVal = static_cast<float>(quality) / 100.0f;  // WIC uses 0.0-1.0 range
        frameProps->Write(1, &qualityProp, &qualityValue);

        hr = frame->Initialize(frameProps.Get());
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = frame->SetSize(width, height);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
        hr = frame->SetPixelFormat(&format);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = frame->WriteSource(converter.Get(), nullptr);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = frame->Commit();
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = encoder->Commit();
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        STATSTG stat{};
        if (FAILED(memStream->Stat(&stat, STATFLAG_NONAME)))
        {
            return std::nullopt;
        }

        const auto size = static_cast<size_t>(stat.cbSize.QuadPart);
        std::vector<uint8_t> jpegData(size);
        LARGE_INTEGER origin{};
        memStream->Seek(origin, STREAM_SEEK_SET, nullptr);
        ULONG bytesRead = 0;
        if (FAILED(memStream->Read(jpegData.data(), static_cast<ULONG>(jpegData.size()), &bytesRead)))
        {
            return std::nullopt;
        }
        jpegData.resize(bytesRead);
        return jpegData;
    }

    /**
     * Detect optimal raster encoding format based on image characteristics
     * Following aviation CGM best practices:
     * - Monochrome (1-bit) -> PNG (JPEG causes ringing artifacts)
     * - Palette/indexed color -> PNG (better compression for limited colors)
     * - Small images -> PNG (JPEG overhead not worthwhile)
     * - Large photographic content -> JPEG
     */
    static SVGConverter::RasterEncoding detectOptimalEncoding(
        const std::vector<std::vector<Color>> &colors,
        int width,
        int height)
    {
        // Rule 1: Small images -> PNG (JPEG overhead not worth it)
        if (width * height < 10000)  // roughly < 100x100
        {
            return SVGConverter::RasterEncoding::PNG;
        }

        // Rule 2: Check for monochrome or low color diversity
        std::unordered_set<uint32_t> uniqueColors;
        size_t sampleCount = 0;
        const size_t maxSamples = 1000;  // Sample up to 1000 pixels for performance

        size_t step = std::max(static_cast<size_t>(1), static_cast<size_t>(width * height) / maxSamples);
        size_t pixelIndex = 0;

        for (int y = 0; y < height && sampleCount < maxSamples; ++y)
        {
            const auto &row = colors[static_cast<size_t>(y)];
            for (int x = 0; x < width && sampleCount < maxSamples; ++x)
            {
                if (pixelIndex % step == 0)
                {
                    const Color &c = (x < static_cast<int>(row.size()))
                        ? row[static_cast<size_t>(x)]
                        : Color::White();
                    uint32_t packed = (static_cast<uint32_t>(c.r) << 16) |
                                      (static_cast<uint32_t>(c.g) << 8) |
                                      static_cast<uint32_t>(c.b);
                    uniqueColors.insert(packed);
                    ++sampleCount;
                }
                ++pixelIndex;
            }
        }

        // Rule 3: If 16 or fewer unique colors in sample, use PNG
        // Aviation CGM rasters are overwhelmingly low-color technical illustrations
        if (uniqueColors.size() <= 16)
        {
            return SVGConverter::RasterEncoding::PNG;
        }

        // Rule 4: If mostly black and white (monochrome-ish), use PNG
        size_t bwCount = 0;
        for (uint32_t c : uniqueColors)
        {
            uint8_t r = (c >> 16) & 0xFF;
            uint8_t g = (c >> 8) & 0xFF;
            uint8_t b = c & 0xFF;
            // Near-black or near-white
            if ((r < 32 && g < 32 && b < 32) || (r > 223 && g > 223 && b > 223))
            {
                ++bwCount;
            }
        }
        if (bwCount > uniqueColors.size() * 8 / 10)  // >80% near-monochrome
        {
            return SVGConverter::RasterEncoding::PNG;
        }

        // Rule 5: High color diversity suggests photographic content -> JPEG
        if (uniqueColors.size() > 200)
        {
            return SVGConverter::RasterEncoding::JPEG;
        }

        // Default to PNG for aviation technical illustrations
        return SVGConverter::RasterEncoding::PNG;
    }

    static std::optional<std::vector<uint8_t>> convertCcittToPng(const std::vector<uint8_t> &ccittData,
                                                                 uint32_t width,
                                                                 uint32_t height,
                                                                 const Color &background,
                                                                 const Color &foreground,
                                                                 bool applyTransparency,
                                                                 const Color &transparentColor,
                                                                 RasterMetrics *metrics = nullptr)
    {
        if (ccittData.empty() || width == 0 || height == 0)
        {
            return std::nullopt;
        }

        IWICImagingFactory *factory = getWicFactory();
        if (!factory)
        {
            return std::nullopt;
        }

        auto tiffBytes = buildCcittTiff(width, height, ccittData);

        ComPtr<IWICStream> inputStream;
        HRESULT hr = factory->CreateStream(&inputStream);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = inputStream->InitializeFromMemory(tiffBytes.data(), static_cast<DWORD>(tiffBytes.size()));
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        ComPtr<IWICBitmapDecoder> decoder;
        hr = factory->CreateDecoderFromStream(inputStream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        ComPtr<IWICFormatConverter> converter;
        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        ComPtr<IWICPalette> palette;
        if (SUCCEEDED(factory->CreatePalette(&palette)))
        {
            palette->InitializePredefined(WICBitmapPaletteTypeFixedGray256, FALSE);
        }

        hr = converter->Initialize(frame.Get(),
                                   GUID_WICPixelFormat8bppGray,
                                   WICBitmapDitherTypeNone,
                                   palette ? palette.Get() : nullptr,
                                   0.0f,
                                   WICBitmapPaletteTypeCustom);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        const UINT strideGray = width;
        std::vector<uint8_t> grayData(static_cast<size_t>(strideGray) * height);
        hr = converter->CopyPixels(nullptr, strideGray, static_cast<UINT>(grayData.size()), grayData.data());
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        const int colorTolerance = applyTransparency ? 1 : 0;
        bool backgroundTransparent = applyTransparency && colorsNearlyEqualRgb(background, transparentColor, colorTolerance);
        bool foregroundTransparent = applyTransparency && colorsNearlyEqualRgb(foreground, transparentColor, colorTolerance);

        size_t backgroundCount = 0;
        size_t foregroundCount = 0;
        if (applyTransparency && (!backgroundTransparent || !foregroundTransparent))
        {
            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    uint8_t gray = grayData[static_cast<size_t>(y) * strideGray + x];
                    bool isForeground = gray < 128;
                    if (isForeground)
                    {
                        ++foregroundCount;
                    }
                    else
                    {
                        ++backgroundCount;
                    }
                }
            }
            if (!backgroundTransparent && !foregroundTransparent)
            {
                if (backgroundCount >= foregroundCount)
                {
                    backgroundTransparent = true;
                }
                else
                {
                    foregroundTransparent = true;
                }
            }
        }

        std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);
        size_t transparentPixels = 0;
        size_t totalPixels = static_cast<size_t>(width) * static_cast<size_t>(height);
        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                uint8_t gray = grayData[static_cast<size_t>(y) * strideGray + x];
                bool isForeground = gray < 128;
                const Color &srcColor = isForeground ? foreground : background;
                bool makeTransparent = (isForeground && foregroundTransparent) ||
                                       (!isForeground && backgroundTransparent);

                size_t idx = (static_cast<size_t>(y) * width + x) * 4;
                rgba[idx + 0] = srcColor.r;
                rgba[idx + 1] = srcColor.g;
                rgba[idx + 2] = srcColor.b;
                rgba[idx + 3] = makeTransparent ? 0 : 255;
                if (makeTransparent)
                {
                    ++transparentPixels;
                }
            }
        }

        if (metrics)
        {
            metrics->transparent_pixels = transparentPixels;
            metrics->opaque_pixels = (totalPixels >= transparentPixels) ? (totalPixels - transparentPixels) : 0;
            metrics->tcc_active = applyTransparency;
            metrics->had_tcc_match = applyTransparency && transparentPixels > 0;
            metrics->pixel_width = static_cast<int>(width);
            metrics->pixel_height = static_cast<int>(height);
            metrics->mime_type = "image/png";
        }

        return encodeRgbaToPng(factory, rgba, width, height);
    }
#else
    static std::optional<std::vector<uint8_t>> convertCcittToPng(const std::vector<uint8_t> &,
                                                                 uint32_t,
                                                                 uint32_t,
                                                                 const Color &,
                                                                 const Color &,
                                                                 bool,
                                                                 const Color &,
                                                                 RasterMetrics *)
    {
        return std::nullopt;
    }
#endif

#ifdef _WIN32
    static std::optional<std::vector<uint8_t>> ensureOpaquePng(const std::vector<uint8_t> &pngData,
                                                               [[maybe_unused]] const Color &backgroundComposite)
    {
        if (pngData.empty())
        {
            return std::nullopt;
        }

        IWICImagingFactory *factory = getWicFactory();
        if (!factory)
        {
            return std::nullopt;
        }

        ComPtr<IWICStream> inputStream;
        HRESULT hr = factory->CreateStream(&inputStream);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = inputStream->InitializeFromMemory(const_cast<BYTE *>(pngData.data()), static_cast<DWORD>(pngData.size()));
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        ComPtr<IWICBitmapDecoder> decoder;
        hr = factory->CreateDecoderFromStream(inputStream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        UINT width = 0;
        UINT height = 0;
        hr = frame->GetSize(&width, &height);
        if (FAILED(hr) || width == 0 || height == 0)
        {
            return std::nullopt;
        }

        ComPtr<IWICFormatConverter> converter;
        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = converter->Initialize(frame.Get(),
                                   GUID_WICPixelFormat32bppRGBA,
                                   WICBitmapDitherTypeNone,
                                   nullptr,
                                   0.0f,
                                   WICBitmapPaletteTypeCustom);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        const UINT stride = width * 4;
        std::vector<uint8_t> rgba(static_cast<size_t>(stride) * height);
        hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(rgba.size()), rgba.data());
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        bool modified = false;
        for (UINT y = 0; y < height; ++y)
        {
            uint8_t *row = &rgba[static_cast<size_t>(y) * stride];
            for (UINT x = 0; x < width; ++x)
            {
                size_t base = static_cast<size_t>(x) * 4;
                uint8_t &a = row[base + 3];
                if (a != 255)
                {
                    a = 255u;
                    modified = true;
                }
            }
        }

        if (!modified)
        {
            return std::nullopt;
        }

        return encodeRgbaToPng(factory, rgba, width, height);
    }
#endif

    static std::string deduceRasterMimeType(const std::vector<uint8_t> &data, int compressionType)
    {
        if (data.size() >= 8 &&
            data[0] == 0x89 && data[1] == 0x50 &&
            data[2] == 0x4E && data[3] == 0x47)
        {
            return "image/png";
        }
        if (data.size() >= 2 && data[0] == 0xFF && data[1] == 0xD8)
        {
            return "image/jpeg";
        }

        switch (compressionType)
        {
        case 6:
            return "image/jpeg";
        case 7:
        case 9:
            return "image/png";
        default:
            return "image/bmp";
        }
    }

#ifdef _WIN32
    // Decode an in-memory BMP via WIC, convert to 32-bit RGBA, and re-encode
    // as PNG. Used for compression-5 (BMP) bitonal tiles so that downstream
    // renderers without BMP-data-URI support (notably resvg) can still
    // display the image.
    static std::optional<std::vector<uint8_t>> convertBmpToPng(const std::vector<uint8_t> &bmpData)
    {
        if (bmpData.empty())
        {
            return std::nullopt;
        }

        IWICImagingFactory *factory = getWicFactory();
        if (!factory)
        {
            return std::nullopt;
        }

        ComPtr<IWICStream> inputStream;
        HRESULT hr = factory->CreateStream(&inputStream);
        if (FAILED(hr)) return std::nullopt;

        hr = inputStream->InitializeFromMemory(const_cast<BYTE *>(bmpData.data()),
                                               static_cast<DWORD>(bmpData.size()));
        if (FAILED(hr)) return std::nullopt;

        ComPtr<IWICBitmapDecoder> decoder;
        hr = factory->CreateDecoderFromStream(inputStream.Get(),
                                              nullptr,
                                              WICDecodeMetadataCacheOnLoad,
                                              &decoder);
        if (FAILED(hr)) return std::nullopt;

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) return std::nullopt;

        UINT width = 0, height = 0;
        hr = frame->GetSize(&width, &height);
        if (FAILED(hr) || width == 0 || height == 0) return std::nullopt;

        ComPtr<IWICFormatConverter> converter;
        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr)) return std::nullopt;

        hr = converter->Initialize(frame.Get(),
                                   GUID_WICPixelFormat32bppRGBA,
                                   WICBitmapDitherTypeNone,
                                   nullptr,
                                   0.0f,
                                   WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) return std::nullopt;

        const UINT stride = width * 4;
        std::vector<uint8_t> rgba(static_cast<size_t>(stride) * height);
        hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(rgba.size()), rgba.data());
        if (FAILED(hr)) return std::nullopt;

        return encodeRgbaToPng(factory, rgba, width, height);
    }

    static std::optional<std::vector<uint8_t>> reencodePngWithTransparency(const std::vector<uint8_t> &pngData,
                                                                           bool applyTransparency,
                                                                           const Color &transparentColor,
                                                                           bool forceRewrite = false)
    {
        if (pngData.empty())
        {
            return std::nullopt;
        }

        IWICImagingFactory *factory = getWicFactory();
        if (!factory)
        {
            return std::nullopt;
        }

        ComPtr<IWICStream> inputStream;
        HRESULT hr = factory->CreateStream(&inputStream);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = inputStream->InitializeFromMemory(const_cast<BYTE *>(pngData.data()), static_cast<DWORD>(pngData.size()));
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        ComPtr<IWICBitmapDecoder> decoder;
        hr = factory->CreateDecoderFromStream(inputStream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        UINT width = 0;
        UINT height = 0;
        hr = frame->GetSize(&width, &height);
        if (FAILED(hr) || width == 0 || height == 0)
        {
            return std::nullopt;
        }

        ComPtr<IWICFormatConverter> converter;
        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        hr = converter->Initialize(frame.Get(),
                                   GUID_WICPixelFormat32bppRGBA,
                                   WICBitmapDitherTypeNone,
                                   nullptr,
                                   0.0f,
                                   WICBitmapPaletteTypeCustom);
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        const UINT stride = width * 4;
        std::vector<uint8_t> rgba(static_cast<size_t>(stride) * height);
        hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(rgba.size()), rgba.data());
        if (FAILED(hr))
        {
            return std::nullopt;
        }

        bool modified = forceRewrite;
        for (UINT y = 0; y < height; ++y)
        {
            uint8_t *row = &rgba[static_cast<size_t>(y) * stride];
            for (UINT x = 0; x < width; ++x)
            {
                size_t idx = static_cast<size_t>(x) * 4;
                Color sample(row[idx + 0], row[idx + 1], row[idx + 2]);
                if (applyTransparency && colorsEqualRgb(sample, transparentColor))
                {
                    if (row[idx + 3] != 0)
                    {
                        row[idx + 3] = 0;
                        modified = true;
                    }
                }
            }
        }

        if (!modified)
        {
            return std::nullopt;
        }

        auto encoded = encodeRgbaToPng(factory, rgba, width, height);
        if (!encoded)
        {
            return std::nullopt;
        }
        return encoded;
    }
#endif

    static double distanceSquared(const CGMPoint &a, const CGMPoint &b)
    {
        const double dx = a.x() - b.x();
        const double dy = a.y() - b.y();
        return dx * dx + dy * dy;
    }

    static double distancePointToSegmentSquared(const CGMPoint &p, const CGMPoint &a, const CGMPoint &b)
    {
        const double lengthSq = distanceSquared(a, b);
        if (lengthSq < 1e-12)
        {
            return distanceSquared(p, a);
        }
        double t = ((p.x() - a.x()) * (b.x() - a.x()) + (p.y() - a.y()) * (b.y() - a.y())) / lengthSq;
        t = std::max(0.0, std::min(1.0, t));
        CGMPoint projection(a.x() + t * (b.x() - a.x()), a.y() + t * (b.y() - a.y()));
        return distanceSquared(p, projection);
    }

    static void adaptiveSampleCurve(const std::function<CGMPoint(double)> &eval,
                                    double t0, double t1,
                                    const CGMPoint &p0, const CGMPoint &p1,
                                    double toleranceSq,
                                    int depth,
                                    std::vector<CGMPoint> &out)
    {
        if (depth > 16)
        {
            out.push_back(p1);
            return;
        }
        double tm = 0.5 * (t0 + t1);
        CGMPoint pm = eval(tm);
        double deviationSq = distancePointToSegmentSquared(pm, p0, p1);
        if (deviationSq > toleranceSq)
        {
            adaptiveSampleCurve(eval, t0, tm, p0, pm, toleranceSq, depth + 1, out);
            adaptiveSampleCurve(eval, tm, t1, pm, p1, toleranceSq, depth + 1, out);
        }
        else
        {
            out.push_back(p1);
        }
    }

    namespace
    {

        // SymbolLibraryDescriptor is now defined in opencgm/svg/internal_types.h
        using SymbolLibraryDescriptor = opencgm::SymbolLibraryDescriptor;

        static std::string trimString(const std::string &value)
        {
            size_t start = 0;
            size_t end = value.size();

            while (start < end && std::isspace(static_cast<unsigned char>(value[start])))
            {
                ++start;
            }
            while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
            {
                --end;
            }
            if (start >= end)
            {
                return std::string();
            }

            std::string trimmed = value.substr(start, end - start);
            if (trimmed.size() >= 2)
            {
                char first = trimmed.front();
                char last = trimmed.back();
                if ((first == '\'' && last == '\'') || (first == '"' && last == '"'))
                {
                    trimmed = trimmed.substr(1, trimmed.size() - 2);
                }
            }
            return trimmed;
        }

        static std::string toLowerCopy(const std::string &value)
        {
            std::string result = value;
            std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return result;
        }

        static bool isLikelyRemoteUri(const std::string &value)
        {
            if (value.empty())
            {
                return false;
            }
            std::string lower = toLowerCopy(value);
            if (lower.rfind("http://", 0) == 0 || lower.rfind("https://", 0) == 0 ||
                lower.rfind("ftp://", 0) == 0 || lower.rfind("mailto:", 0) == 0 ||
                lower.rfind("data:", 0) == 0)
            {
                return true;
            }
            return lower.find("://") != std::string::npos;
        }

        static SymbolLibraryDescriptor parseSymbolDescriptor(const std::string &raw)
        {
            SymbolLibraryDescriptor descriptor;
            descriptor.raw = raw;

            std::string trimmed = trimString(raw);
            if (trimmed.empty())
            {
                return descriptor;
            }

            size_t delimiterPos = std::string::npos;
            for (char candidate : {'|', '='})
            {
                size_t pos = trimmed.find(candidate);
                if (pos != std::string::npos)
                {
                    delimiterPos = pos;
                    break;
                }
            }

            if (delimiterPos != std::string::npos)
            {
                descriptor.label = trimString(trimmed.substr(0, delimiterPos));
                descriptor.uri = trimString(trimmed.substr(delimiterPos + 1));
            }
            else
            {
                descriptor.label = trimmed;
                descriptor.uri = trimmed;
            }

            if (descriptor.uri.empty())
            {
                descriptor.uri = descriptor.label;
            }

            size_t hashPos = descriptor.uri.find('#');
            if (hashPos != std::string::npos)
            {
                descriptor.fragment = trimString(descriptor.uri.substr(hashPos + 1));
                descriptor.uri = trimString(descriptor.uri.substr(0, hashPos));
            }

            if (descriptor.fragment.empty() && !descriptor.label.empty() &&
                descriptor.label != descriptor.uri)
            {
                descriptor.fragment = descriptor.label;
            }

            return descriptor;
        }

        static std::string escapeRegex(const std::string &value)
        {
            static const std::string specials = R"(\.^$|()[]{}*+?!)";
            std::string result;
            result.reserve(value.size() * 2);
            for (char ch : value)
            {
                if (specials.find(ch) != std::string::npos)
                {
                    result.push_back('\\');
                }
                result.push_back(ch);
            }
            return result;
        }

        static std::optional<std::string> resolveSymbolLibraryPath(
            const SymbolLibraryDescriptor &descriptor,
            const std::string &baseDir)
        {

            if (descriptor.uri.empty() || isLikelyRemoteUri(descriptor.uri))
            {
                return std::nullopt;
            }

            std::vector<std::filesystem::path> candidates;
            std::unordered_set<std::string> seen;
            std::filesystem::path uriPath(descriptor.uri);

            auto addCandidate = [&](const std::filesystem::path &candidate)
            {
                if (candidate.empty())
                {
                    return;
                }
                std::string key = candidate.generic_string();
                if (seen.insert(key).second)
                {
                    candidates.push_back(candidate);
                }
            };

            if (uriPath.is_absolute())
            {
                addCandidate(uriPath);
            }

            if (!baseDir.empty())
            {
                std::filesystem::path base(baseDir);
                addCandidate(base / uriPath);
                addCandidate(base / "symbols" / uriPath);
                addCandidate(base / "symbol-libraries" / uriPath);
            }

            addCandidate(uriPath);

            if (uriPath.extension().empty())
            {
                std::filesystem::path withSvg = uriPath;
                withSvg += ".svg";
                if (uriPath.is_absolute())
                {
                    addCandidate(withSvg);
                }
                addCandidate(withSvg);
                if (!baseDir.empty())
                {
                    std::filesystem::path base(baseDir);
                    addCandidate(base / withSvg);
                    addCandidate(base / "symbols" / withSvg);
                    addCandidate(base / "symbol-libraries" / withSvg);
                }
            }

            for (const auto &candidate : candidates)
            {
                std::error_code ec;
                if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec))
                {
                    return candidate.string();
                }
            }

            return std::nullopt;
        }

        static std::string removeIdAttribute(const std::string &attributes)
        {
            std::string result;
            size_t i = 0;
            while (i < attributes.size())
            {
                while (i < attributes.size() && std::isspace(static_cast<unsigned char>(attributes[i])))
                {
                    ++i;
                }
                if (i >= attributes.size())
                {
                    break;
                }
                size_t nameStart = i;
                while (i < attributes.size() &&
                       !std::isspace(static_cast<unsigned char>(attributes[i])) &&
                       attributes[i] != '=')
                {
                    ++i;
                }
                std::string name = attributes.substr(nameStart, i - nameStart);
                std::string lowerName = toLowerCopy(name);
                while (i < attributes.size() && std::isspace(static_cast<unsigned char>(attributes[i])))
                {
                    ++i;
                }
                bool hasEquals = false;
                std::string value;
                if (i < attributes.size() && attributes[i] == '=')
                {
                    hasEquals = true;
                    ++i;
                    while (i < attributes.size() && std::isspace(static_cast<unsigned char>(attributes[i])))
                    {
                        ++i;
                    }
                    if (i < attributes.size() && (attributes[i] == '"' || attributes[i] == '\''))
                    {
                        char quote = attributes[i++];
                        size_t valueStart = i;
                        while (i < attributes.size() && attributes[i] != quote)
                        {
                            ++i;
                        }
                        value = attributes.substr(valueStart, i - valueStart);
                        if (i < attributes.size())
                        {
                            ++i;
                        }
                    }
                    else
                    {
                        size_t valueStart = i;
                        while (i < attributes.size() && !std::isspace(static_cast<unsigned char>(attributes[i])))
                        {
                            ++i;
                        }
                        value = attributes.substr(valueStart, i - valueStart);
                    }
                }

                if (lowerName == "id" || lowerName == "xml:id")
                {
                    continue;
                }

                if (!result.empty())
                {
                    result.push_back(' ');
                }
                result += name;
                if (hasEquals)
                {
                    result += "=\"";
                    result += value;
                    result += "\"";
                }
            }
            return result;
        }

        static std::optional<std::string> buildSymbolMarkup(
            const std::string &xmlContent,
            const SymbolLibraryDescriptor &descriptor)
        {

            if (descriptor.fragment.empty())
            {
                return std::nullopt;
            }

            std::string escapedFragment = escapeRegex(descriptor.fragment);
            std::regex elementPattern(
                "<(symbol|g)\\b(?=[^>]*\\bid\\s*=\\s*(['\"])" + escapedFragment + "\\2)([^>]*)>([\\s\\S]*?)</\\1>",
                std::regex::icase);

            std::smatch match;
            if (std::regex_search(xmlContent, match, elementPattern))
            {
                std::string tagName = match[1].str();
                std::string attributes = removeIdAttribute(match[3].str());
                std::string innerContent = match[4].str();

                std::ostringstream oss;
                if (toLowerCopy(tagName) == "symbol")
                {
                    oss << "<g";
                }
                else
                {
                    oss << "<" << tagName;
                }
                if (!attributes.empty())
                {
                    oss << " " << trimString(attributes);
                }
                oss << ">";
                oss << innerContent;
                if (toLowerCopy(tagName) == "symbol")
                {
                    oss << "</g>";
                }
                else
                {
                    oss << "</" << tagName << ">";
                }
                return oss.str();
            }

            std::regex selfClosingPattern(
                "<(symbol|g)\\b(?=[^>]*\\bid\\s*=\\s*(['\"])" + escapedFragment + "\\2)([^>]*)/\\s*>",
                std::regex::icase);

            if (std::regex_search(xmlContent, match, selfClosingPattern))
            {
                std::string tagName = match[1].str();
                std::string attributes = removeIdAttribute(match[3].str());
                std::ostringstream oss;
                if (toLowerCopy(tagName) == "symbol")
                {
                    oss << "<g";
                }
                else
                {
                    oss << "<" << tagName;
                }
                if (!attributes.empty())
                {
                    oss << " " << trimString(attributes);
                }
                oss << " />";
                return oss.str();
            }

            return std::nullopt;
        }

        static std::string indentMultiline(const std::string &text, int spaces)
        {
            if (text.empty())
            {
                return std::string();
            }
            std::string indent(static_cast<size_t>(spaces), ' ');
            std::ostringstream oss;
            size_t start = 0;
            while (start < text.size())
            {
                size_t end = text.find('\n', start);
                if (end == std::string::npos)
                {
                    end = text.size();
                }
                if (end > start)
                {
                    oss << indent << text.substr(start, end - start);
                }
                else
                {
                    oss << indent;
                }
                if (end < text.size())
                {
                    oss << "\n";
                }
                start = end + 1;
            }
            return oss.str();
        }

    } // namespace

    void SVGConverter::setSegmentPolicy(SegmentPolicy policy)
    {
        segment_policy_ = policy;
    }

    void SVGConverter::setPaletteOverride(const PaletteOverride &overrideConfig)
    {
        palette_override_ = overrideConfig;
    }

    void SVGConverter::clearPaletteOverride()
    {
        palette_override_ = PaletteOverride{};
    }

    void SVGConverter::setColorLoggingEnabled(bool enabled)
    {
        color_logging_enabled_ = enabled;
        if (!color_logging_enabled_)
        {
            last_logged_colors_.clear();
        }
    }

    void SVGConverter::setWidthLoggingEnabled(bool enabled)
    {
        width_logging_enabled_ = enabled;
    }

    void SVGConverter::setRasterLoggingEnabled(bool enabled)
    {
        raster_logging_enabled_ = enabled;
        if (!raster_logging_enabled_)
        {
            raster_metrics_.clear();
        }
    }

    void SVGConverter::setTransparentCellColourEnabled(bool enabled)
    {
        transparent_cell_colour_enabled_ = enabled;
        if (!enabled)
        {
            transparent_cell_active_ = false;
        }
    }

    void SVGConverter::setAttributeOutputFormat(svg::AttributeManager::OutputFormat format)
    {
        pending_conversion_plan_.attribute_format = format;
    }

    void SVGConverter::setCompatibilityMode(bool enabled)
    {
        if (compatibility_mode_ != enabled)
        {
            compatibility_mode_ = enabled;
            document_metrics_dirty_ = true;
        }
    }

    void SVGConverter::setGeometryLoggingEnabled(bool enabled)
    {
        geometry_logging_enabled_ = enabled;
    }

    void SVGConverter::setPngQuantizationEnabled(bool enabled)
    {
        png_quantization_enabled_ = enabled;
    }

    void SVGConverter::setViewboxPaddingFraction(double fraction)
    {
        double oldFraction = viewbox_padding_fraction_;
        if (!std::isfinite(fraction) || fraction < 0.0)
        {
            viewbox_padding_fraction_ = 0.0;
        }
        else if (fraction > 0.2)
        {
            viewbox_padding_fraction_ = 0.2;
        }
        else
        {
            viewbox_padding_fraction_ = fraction;
        }

        if (oldFraction != viewbox_padding_fraction_)
        {
            document_metrics_dirty_ = true;
        }
    }

    void SVGConverter::setNurbsToleranceSvgUnits(double svgUnits)
    {
        if (!std::isfinite(svgUnits))
        {
            return;
        }
        nurbs_tolerance_svg_units_ =
            std::max(0.001, std::min(10.0, svgUnits));
    }

    void SVGConverter::setOutputProfile(OutputProfile profile)
    {
        const OutputTargetConfig customTarget =
            pending_conversion_plan_.output_target;
        const HotspotProfileConfig hotspot =
            pending_conversion_plan_.hotspot;
        pending_conversion_plan_ =
            ResolvedConversionPlan::forProfile(profile);
        pending_conversion_plan_.hotspot = hotspot;
        if (profile == OutputProfile::Custom)
        {
            pending_conversion_plan_.output_target = customTarget;
        }

        document_metrics_dirty_ = true;
#ifndef NDEBUG
        if (const char *debugBounds = std::getenv("SVG_DEBUG_BOUNDS"))
        {
            (void)debugBounds;
            std::cerr << "[svg] setOutputProfile profile=" << static_cast<int>(profile) << "\n";
        }
#endif
    }

    void SVGConverter::setConversionPlan(const ConversionPlan &plan)
    {
        pending_conversion_plan_ = plan;
        document_metrics_dirty_ = true;
    }

    OutputTargetConfig SVGConverter::getOutputTargetConfig() const
    {
        return pending_conversion_plan_.output_target;
    }

    SVGConverter::SVGConverter(CGMFile *cgm_file) : cgm_file_(cgm_file),
                                                     svg_width_(800),
                                                     svg_height_(600),
                                                     source_file_path_(),
                                                     source_file_hash_(),
                                                     source_hash_computed_(false),
                                                     current_picture_name_(),
                                                     png_quantization_enabled_(false),
                                                     clip_path_cache_(),
                                                     background_color_(Color::White()),
                                                     background_color_explicit_(false),
                                                     palette_override_(),
                                                     color_logging_enabled_(false),
                                                     width_logging_enabled_(false),
                                                     viewbox_padding_fraction_(0.0),
                                                     conversion_plan_(ResolvedConversionPlan::forProfile(OutputProfile::WebCGM21)),
                                                     pending_conversion_plan_(conversion_plan_),
                                                     line_width_spec_mode_(SpecificationMode::ABS),
                                                     edge_width_spec_mode_(SpecificationMode::ABS),
                                                     abstract_line_width_unit_(1.0),
                                                     abstract_edge_width_unit_(1.0),
                                                     nominal_line_width_svg_(1.0),
                                                     nominal_edge_width_svg_(1.0),
                                                     scaling_mode_(SpecificationMode::ABS),
                                                     metric_scale_factor_(1.0),
                                                     picture_scale_x_(1.0),
                                                     picture_scale_y_(1.0),
                                                     picture_vdc_width_(1.0),
                                                     picture_vdc_height_(1.0),
                                                     view_context_active_(false),
                                                     view_context_x1_(0.0),
                                                     view_context_y1_(0.0),
                                                     view_context_x2_(0.0),
                                                     view_context_y2_(0.0),
                                                     view_context_count_(0),
                                                     view_context_multiple_(false),
                                                     apply_view_context_on_load_(false),
                                                     view_context_nvdc_valid_(false),
                                                     view_context_nvdc_x1_(0.0),
                                                     view_context_nvdc_y1_(0.0),
                                                     view_context_nvdc_x2_(0.0),
                                                     view_context_nvdc_y2_(0.0),
                                                     picture_vdc_width_raw_(1.0),
                                                     picture_vdc_height_raw_(1.0),
                                                     picture_longest_side_raw_(1.0),
                                                     picture_vdc_min_x_(0.0),
                                                     picture_vdc_min_y_(0.0),
                                                     picture_vdc_max_x_(0.0),
                                                     picture_vdc_max_y_(0.0),
                                                     picture_vdc_y_down_(false),
                                                     svg_canvas_width_(1.0),
                                                     svg_canvas_height_(1.0),
                                                     svg_viewbox_x_(0.0),
                                                     svg_viewbox_y_(0.0),
                                                     svg_viewbox_width_(1.0),
                                                     svg_viewbox_height_(1.0),
                                                     expected_colour_table_size_(256),
                                                     colour_value_extent_min_(Color::Black()),
                                                     colour_value_extent_max_(Color::White()),
                                                     line_color_slots_(Color::Black(), Color::Black()),
                                                     fill_color_slots_(Color::Black(), Color::Black()),
                                                     edge_color_slots_(Color::Black(), Color::Black()),
                                                     text_color_slots_(Color::Black(), Color::Black()),
                                                     active_color_mode_(ColorSelectionMode::INDEXED),
                                                     in_figure_(false),
                                                     figure_connects_subpaths_(false),
                                                     in_tile_array_(false),
                                                     tile_array_position_(0.0, 0.0),
                                                     tile_path_direction_(0),
                                                     tile_line_direction_(90),
                                                     tile_cell_width_(1.0),
                                                     tile_cell_height_(1.0),
                                                     tile_tiles_in_path_(1),
                                                     tile_tiles_in_line_(1),
                                                     tile_cells_per_tile_path_(1),
                                                     tile_cells_per_tile_line_(1),
                                                     tile_image_offset_path_(0),
                                                     tile_image_offset_line_(0),
                                                     tile_image_cells_path_(0),
                                                     tile_image_cells_line_(0),
                                                     tile_current_index_(0),
                                                     symbol_placeholder_counter_(0),
                                                     gdp_placeholder_counter_(0),
                                                     defs_open_(false),
                                                     pattern_counter_(0),
                                                     active_pattern_size_(),
                                                     has_active_pattern_size_(false),
                                                     fill_reference_point_(0.0, 0.0),
                                                     has_fill_reference_point_(false),
                                                     last_defined_pattern_index_(1),
                                                     fill_bundles_(),
                                                     active_fill_bundle_index_(1),
                                                     clip_rectangle_defined_(false),
                                                     clip_rect_first_(0.0, 0.0),
                                                     clip_rect_second_(0.0, 0.0),
                                                     clip_enabled_(false),
                                                     clip_path_attribute_(),
                                                     clip_path_counter_(0),
                                                     in_protection_region_(false),
                                                     protection_region_indicator_(1),
                                                     active_protection_region_index_(-1),
                                                     current_command_index_(0),
                                                     debug_fill_logging_(false),
                                                     transparent_cell_active_(false),
                                                     transparent_cell_color_(Color::White()),
                                                     viewer_shim_mode_(ViewerShimMode::Auto),
                                                     viewer_shim_url_(),
                                                     compress_output_(false),
                                                     raster_encoding_(RasterEncoding::Auto),
                                                     jpeg_quality_(85),
                                                     jpeg_444_subsampling_(true),
                                                     has_layer_aps_(false),
                                                     has_linkuri_aps_(false),
                                                     has_viewcontext_aps_(false),
                                                     pending_text_active_(false),
                                                     pending_text_position_(0.0, 0.0),
                                                     pending_text_segments_(),
                                                     pending_text_h_align_(1),
                                                     pending_text_v_align_(5),
                                                     has_last_text_position_(false),
                                                     last_text_position_(0.0, 0.0),
                                                     pending_text_rotation_deg_(0.0),
                                                     pending_text_has_rotation_(false),
                                                     pending_is_restricted_text_(false),
                                                     pending_restricted_delta_width_(0.0),
                                                     pending_restricted_delta_height_(0.0),
                                                     character_orientation_base_(1.0, 0.0),
                                                     character_orientation_up_(0.0, 1.0),
                                                     segment_policy_(SegmentPolicy::StrictWebCgm),
                                                     encountered_segment_(false),
                                                     current_picture_index_(-1),
                                                     text_as_path_warning_emitted_(false),
                                                     transparent_cell_colour_enabled_(true),
                                                     compatibility_mode_(false),
                                                     geometry_logging_enabled_(false),
                                                     geometry_metrics_(),
                                                     raster_logging_enabled_(false)
    {
#ifndef NDEBUG
        const char *debugFillEnv = std::getenv("SVG_DEBUG_FILL");
        debug_fill_logging_ = (debugFillEnv != nullptr && debugFillEnv[0] != '\0' && debugFillEnv[0] != '0');
        if (debug_fill_logging_)
        {
            std::cerr << "[svg] debug fill logging enabled\n";
        }
        if (const char *logEnv = std::getenv("SVG_LOG_COLORS"))
        {
            color_logging_enabled_ = true;
            if (logEnv[0] != '\0')
            {
                std::cerr << "[svg] colour logging enabled\n";
            }
        }
        if (std::getenv("SVG_LOG_WIDTHS") != nullptr)
        {
            width_logging_enabled_ = true;
            std::cerr << "[svg] width logging enabled\n";
        }
#else
        debug_fill_logging_ = false;
#endif

        const std::string &fileName = cgm_file_->fileName();
        if (!fileName.empty())
        {
            source_file_path_ = fileName;
            std::filesystem::path filePath(fileName);
            if (filePath.has_parent_path())
            {
                cgm_base_dir_ = filePath.parent_path().string();
            }
            else
            {
                cgm_base_dir_.clear();
            }
        }
        if (const char *emitHashEnv = std::getenv("OPENCGM_EMIT_SOURCE_HASH"))
        {
            emit_source_hash_ = (emitHashEnv[0] != '\0' && emitHashEnv[0] != '0');
        }
        initializeDefaultFontOptions();
    }

    void SVGConverter::recomputeDocumentMetrics()
    {
        if (!cgm_file_)
        {
            document_metrics_dirty_ = false;
            return;
        }

        background_color_ = Color::White();
        background_color_explicit_ = false;
        transparent_cell_active_ = false;
        transparent_cell_color_ = Color::White();
        line_width_spec_mode_ = SpecificationMode::ABS;
        edge_width_spec_mode_ = SpecificationMode::ABS;
        scaling_mode_ = cgm_file_->scalingMode();
        metric_scale_factor_ = cgm_file_->metricScaleFactor();
        view_context_active_ = false;
        view_context_x1_ = view_context_y1_ = 0.0;
        view_context_x2_ = view_context_y2_ = 0.0;
        view_context_count_ = 0;
        view_context_multiple_ = false;
        apply_view_context_on_load_ = false;
        view_context_nvdc_valid_ = false;
        view_context_nvdc_x1_ = view_context_nvdc_y1_ = 0.0;
        view_context_nvdc_x2_ = view_context_nvdc_y2_ = 0.0;
        picture_vdc_min_x_ = 0.0;
        picture_vdc_min_y_ = 0.0;
        picture_vdc_max_x_ = 0.0;
        picture_vdc_max_y_ = 0.0;
        picture_vdc_orig_x1_ = 0.0;
        picture_vdc_orig_y1_ = 0.0;
        picture_vdc_orig_x2_ = 0.0;
        picture_vdc_orig_y2_ = 0.0;
        picture_vdc_x_left_ = false;
        picture_vdc_y_down_ = false;

        bool vdc_extent_found = false;
        double vdc_x1 = 0.0;
        double vdc_y1 = 0.0;
        double vdc_x2 = 32767.0;
        double vdc_y2 = 32767.0;

        for (const auto &cmd : cgm_file_->commands())
        {
            if (cmd->elementClass() != ClassCode::PictureDescriptorElements)
            {
                continue;
            }

            switch (cmd->elementId())
            {
            case 6: // VDC EXTENT
            {
                auto *extent = metricsCast<VDCExtent, ClassCode::PictureDescriptorElements, 6>(cmd.get());
                if (extent)
                {
                    vdc_x1 = extent->firstCorner().x();
                    vdc_y1 = extent->firstCorner().y();
                    vdc_x2 = extent->secondCorner().x();
                    vdc_y2 = extent->secondCorner().y();
                    vdc_extent_found = true;
                }
                break;
            }
            case 7: // BACKGROUND COLOUR
            {
                auto *bg = metricsCast<BackgroundColour, ClassCode::PictureDescriptorElements, 7>(cmd.get());
                if (bg)
                {
                    background_color_ = bg->color();
                    background_color_explicit_ = true;
                }
                break;
            }
            case 3: // LINE WIDTH SPECIFICATION MODE
            {
                auto *mode = metricsCast<LineWidthSpecificationMode, ClassCode::PictureDescriptorElements, 3>(cmd.get());
                if (mode)
                {
                    line_width_spec_mode_ = mode->mode();
                }
                break;
            }
            case 5: // EDGE WIDTH SPECIFICATION MODE
            {
                auto *mode = metricsCast<EdgeWidthSpecificationMode, ClassCode::PictureDescriptorElements, 5>(cmd.get());
                if (mode)
                {
                    edge_width_spec_mode_ = mode->mode();
                }
                break;
            }
            default:
                break;
            }
        }

        bool has_points = false;
        double min_x = 0.0;
        double min_y = 0.0;
        double max_x = 0.0;
        double max_y = 0.0;

        auto updateBounds = [&](double x, double y)
        {
            if (!has_points)
            {
                min_x = max_x = x;
                min_y = max_y = y;
                has_points = true;
                return;
            }
            if (x < min_x)
            {
                min_x = x;
            }
            if (x > max_x)
            {
                max_x = x;
            }
            if (y < min_y)
            {
                min_y = y;
            }
            if (y > max_y)
            {
                max_y = y;
            }
        };

        auto accumulateEllipseBounds = [&](const CGMPoint &center,
                                           const CGMPoint &first,
                                           const CGMPoint &second)
        {
            CGMPoint u = subtractPoints(first, center);
            CGMPoint v = subtractPoints(second, center);
            double span_x = std::hypot(u.x(), v.x());
            double span_y = std::hypot(u.y(), v.y());
            if (!std::isfinite(span_x) || !std::isfinite(span_y))
            {
                return;
            }
            updateBounds(center.x() - span_x, center.y() - span_y);
            updateBounds(center.x() + span_x, center.y() + span_y);
        };

        auto accumulateEllipseArcBounds = [&](const CGMPoint &center,
                                              const CGMPoint &first,
                                              const CGMPoint &second,
                                              const CGMPoint &startDelta,
                                              const CGMPoint &endDelta,
                                              int closure)
        {
            accumulateEllipseBounds(center, first, second);
            CGMPoint startPoint = addPoints(center, startDelta);
            CGMPoint endPoint = addPoints(center, endDelta);
            updateBounds(startPoint.x(), startPoint.y());
            updateBounds(endPoint.x(), endPoint.y());
            if (closure == 0)
            {
                updateBounds(center.x(), center.y());
            }
        };

        for (const auto &cmd : cgm_file_->commands())
        {
            if (cmd->elementClass() != ClassCode::GraphicalPrimitiveElements)
            {
                continue;
            }

            const int elementId = cmd->elementId();
            switch (elementId)
            {
            case 1: // Polyline
            {
                auto *polyline = metricsCast<Polyline, ClassCode::GraphicalPrimitiveElements, 1>(cmd.get());
                if (polyline)
                {
                    for (const auto &pt : polyline->points())
                    {
                        updateBounds(pt.x(), pt.y());
                    }
                }
                break;
            }
            case 7: // Polygon
            {
                auto *polygon = metricsCast<Polygon, ClassCode::GraphicalPrimitiveElements, 7>(cmd.get());
                if (polygon)
                {
                    for (const auto &pt : polygon->points())
                    {
                        updateBounds(pt.x(), pt.y());
                    }
                }
                break;
            }
            case 12: // Circle
            {
                auto *circle = metricsCast<Circle, ClassCode::GraphicalPrimitiveElements, 12>(cmd.get());
                if (circle)
                {
                    double cx = circle->center().x();
                    double cy = circle->center().y();
                    double r = circle->radius();
                    updateBounds(cx - r, cy - r);
                    updateBounds(cx + r, cy + r);
                }
                break;
            }
            case 17: // Ellipse
            {
                auto *ellipse = metricsCast<Ellipse, ClassCode::GraphicalPrimitiveElements, 17>(cmd.get());
                if (ellipse)
                {
                    accumulateEllipseBounds(
                        ellipse->center(),
                        ellipse->firstConjugateDiameter(),
                        ellipse->secondConjugateDiameter());
                }
                break;
            }
            case 18: // Elliptical Arc
            {
                auto *arc = metricsCast<EllipticalArc, ClassCode::GraphicalPrimitiveElements, 18>(cmd.get());
                if (arc)
                {
                    accumulateEllipseArcBounds(
                        arc->center(),
                        arc->firstConjugate(),
                        arc->secondConjugate(),
                        arc->startDelta(),
                        arc->endDelta(),
                        1);
                }
                break;
            }
            case 19: // Elliptical Arc Close
            {
                auto *arcClose = metricsCast<EllipticalArcClose, ClassCode::GraphicalPrimitiveElements, 19>(cmd.get());
                if (arcClose)
                {
                    accumulateEllipseArcBounds(
                        arcClose->center(),
                        arcClose->firstConjugate(),
                        arcClose->secondConjugate(),
                        arcClose->startDelta(),
                        arcClose->endDelta(),
                        arcClose->closure());
                }
                break;
            }
            case 11: // Rectangle
            {
                auto *rect = metricsCast<Rectangle, ClassCode::GraphicalPrimitiveElements, 11>(cmd.get());
                if (rect)
                {
                    updateBounds(rect->firstCorner().x(), rect->firstCorner().y());
                    updateBounds(rect->secondCorner().x(), rect->secondCorner().y());
                }
                break;
            }
            case 4: // Text
            {
                auto *text = metricsCast<Text, ClassCode::GraphicalPrimitiveElements, 4>(cmd.get());
                if (text)
                {
                    updateBounds(text->position().x(), text->position().y());
                }
                break;
            }
            case 5: // Restricted Text
            {
                auto *restricted = metricsCast<RestrictedText, ClassCode::GraphicalPrimitiveElements, 5>(cmd.get());
                if (restricted)
                {
                    updateBounds(restricted->position().x(), restricted->position().y());
                }
                break;
            }
            case 15: // Circular Arc Centre
            {
                auto *arc = metricsCast<CircularArcCentre, ClassCode::GraphicalPrimitiveElements, 15>(cmd.get());
                if (arc)
                {
                    double cx = arc->center().x();
                    double cy = arc->center().y();
                    double r = arc->radius();
                    updateBounds(cx - r, cy - r);
                    updateBounds(cx + r, cy + r);
                }
                break;
            }
            case 16: // Circular Arc Centre Close
            {
                auto *arc = metricsCast<CircularArcCentreClose, ClassCode::GraphicalPrimitiveElements, 16>(cmd.get());
                if (arc)
                {
                    double cx = arc->center().x();
                    double cy = arc->center().y();
                    double r = arc->radius();
                    updateBounds(cx - r, cy - r);
                    updateBounds(cx + r, cy + r);
                }
                break;
            }
            case 26: // PolyBezier
            {
                auto *bezier = metricsCast<PolyBezier, ClassCode::GraphicalPrimitiveElements, 26>(cmd.get());
                if (bezier)
                {
                    for (const auto &pt : bezier->controlPoints())
                    {
                        updateBounds(pt.x(), pt.y());
                    }
                }
                break;
            }
            default:
                break;
            }
        }

        for (const auto &cmd : cgm_file_->defaultsReplacementCommands())
        {
            if (auto *lineMode = metricsCast<LineWidthSpecificationMode, ClassCode::PictureDescriptorElements, 3>(cmd.get()))
            {
                line_width_spec_mode_ = lineMode->mode();
            }
            else if (auto *edgeMode = metricsCast<EdgeWidthSpecificationMode, ClassCode::PictureDescriptorElements, 5>(cmd.get()))
            {
                edge_width_spec_mode_ = edgeMode->mode();
            }
        }

        double picture_x1 = vdc_extent_found ? vdc_x1 : 0.0;
        double picture_y1 = vdc_extent_found ? vdc_y1 : 0.0;
        double picture_x2 = vdc_extent_found ? vdc_x2 : 32767.0;
        double picture_y2 = vdc_extent_found ? vdc_y2 : 32767.0;

        double bounds_x1 = picture_x1;
        double bounds_y1 = picture_y1;
        double bounds_x2 = picture_x2;
        double bounds_y2 = picture_y2;

        if (has_points)
        {
            bounds_x1 = std::min(min_x, max_x);
            bounds_y1 = std::min(min_y, max_y);
            bounds_x2 = std::max(min_x, max_x);
            bounds_y2 = std::max(min_y, max_y);
        }
        else if (!vdc_extent_found)
        {
            bounds_x1 = 0.0;
            bounds_y1 = 0.0;
            bounds_x2 = 32767.0;
            bounds_y2 = 32767.0;
        }

        if (bounds_x1 == bounds_x2)
        {
            bounds_x2 = bounds_x1 + 1.0;
        }
        if (bounds_y1 == bounds_y2)
        {
            bounds_y2 = bounds_y1 + 1.0;
        }

        double geometry_width = has_points ? std::abs(bounds_x2 - bounds_x1) : 0.0;
        double geometry_height = has_points ? std::abs(bounds_y2 - bounds_y1) : 0.0;

        double picture_width = std::abs(picture_x2 - picture_x1);
        double picture_height = std::abs(picture_y2 - picture_y1);
        if (picture_width <= 0.0 || picture_height <= 0.0)
        {
            picture_x1 = bounds_x1;
            picture_y1 = bounds_y1;
            picture_x2 = bounds_x2;
            picture_y2 = bounds_y2;
            picture_width = std::abs(picture_x2 - picture_x1);
            picture_height = std::abs(picture_y2 - picture_y1);
        }

        picture_vdc_width_raw_ = picture_width;
        picture_vdc_height_raw_ = picture_height;
        picture_longest_side_raw_ = std::max(picture_vdc_width_raw_, picture_vdc_height_raw_);
        if (picture_longest_side_raw_ <= 0.0)
        {
            picture_longest_side_raw_ = std::max(std::abs(bounds_x2 - bounds_x1), std::abs(bounds_y2 - bounds_y1));
        }
        if (picture_longest_side_raw_ <= 0.0)
        {
            picture_longest_side_raw_ = 1.0;
        }

        picture_vdc_width_ = picture_width;
        picture_vdc_height_ = picture_height;

        double picture_min_x = std::min(picture_x1, picture_x2);
        double picture_max_x = std::max(picture_x1, picture_x2);
        double picture_min_y = std::min(picture_y1, picture_y2);
        double picture_max_y = std::max(picture_y1, picture_y2);
        picture_vdc_min_x_ = picture_min_x;
        picture_vdc_max_x_ = picture_max_x;
        picture_vdc_min_y_ = picture_min_y;
        picture_vdc_max_y_ = picture_max_y;
        // Store original VDC extent preserving axis order (for coordinate transformation)
        picture_vdc_orig_x1_ = picture_x1;
        picture_vdc_orig_y1_ = picture_y1;
        picture_vdc_orig_x2_ = picture_x2;
        picture_vdc_orig_y2_ = picture_y2;
        picture_vdc_x_left_ = (picture_x1 > picture_x2);  // X decreases left-to-right
        picture_vdc_y_down_ = (picture_y1 > picture_y2);  // Y increases downward

        auto assignViewContextRect = [&](double rawX1, double rawY1, double rawX2, double rawY2) -> bool
        {
            if (view_context_count_ > 0)
            {
                view_context_multiple_ = true;
                return false;
            }

            const auto ordered = svg::ApsGeometry::orderedRect(
                rawX1,
                rawY1,
                rawX2,
                rawY2);
            const auto clamped = ordered
                ? svg::ApsGeometry::clampRect(
                      *ordered,
                      svg::ApsRect{
                          picture_min_x,
                          picture_min_y,
                          picture_max_x,
                          picture_max_y})
                : std::nullopt;
            if (!clamped)
            {
                return false;
            }

            ++view_context_count_;
            view_context_active_ = true;
            view_context_x1_ = clamped->min_x;
            view_context_y1_ = clamped->min_y;
            view_context_x2_ = clamped->max_x;
            view_context_y2_ = clamped->max_y;

            const auto nvdc = svg::ApsGeometry::toNvdc(
                *clamped,
                scaling_mode_ == SpecificationMode::SCALED,
                metric_scale_factor_,
                picture_vdc_min_x_,
                picture_vdc_min_y_,
                picture_vdc_max_y_,
                picture_vdc_y_down_);
            if (nvdc)
            {
                view_context_nvdc_valid_ = true;
                view_context_nvdc_x1_ = nvdc->min_x;
                view_context_nvdc_y1_ = nvdc->min_y;
                view_context_nvdc_x2_ = nvdc->max_x;
                view_context_nvdc_y2_ = nvdc->max_y;
            }
            else
            {
                view_context_nvdc_valid_ = false;
            }
            return true;
        };

        auto parseViewContextTokens = [&](const std::vector<std::string> &tokens) -> bool
        {
            if (view_context_count_ > 0)
            {
                view_context_multiple_ = true;
                return false;
            }
            const auto rect =
                svg::ApsGeometry::parseRectTokens(tokens);
            return rect &&
                   assignViewContextRect(
                       rect->min_x,
                       rect->min_y,
                       rect->max_x,
                       rect->max_y);
        };

        if (!view_context_active_)
        {
            std::vector<std::string> aps_identifier_stack;
            std::vector<std::string> aps_type_stack;

            for (const auto &cmd : cgm_file_->commands())
            {
                if (cmd->elementClass() == ClassCode::DelimiterElement)
                {
                    if (cmd->elementId() == 21)
                    {
                        if (auto *beginAps = metricsCast<BeginApplicationStructure, ClassCode::DelimiterElement, 21>(cmd.get()))
                        {
                            aps_identifier_stack.push_back(beginAps->identifier());
                            aps_type_stack.push_back(beginAps->type());
                        }
                        continue;
                    }
                    if (cmd->elementId() == 23)
                    {
                        if (!aps_identifier_stack.empty())
                        {
                            aps_identifier_stack.pop_back();
                        }
                        if (!aps_type_stack.empty())
                        {
                            aps_type_stack.pop_back();
                        }
                        continue;
                    }
                }

                if (view_context_active_)
                {
                    break;
                }

                if (cmd->elementClass() != ClassCode::ApplicationStructureDescriptorElements || cmd->elementId() != 1)
                {
                    continue;
                }

                auto *attr = metricsCast<ApplicationStructureAttribute, ClassCode::ApplicationStructureDescriptorElements, 1>(cmd.get());
                if (!attr)
                {
                    continue;
                }

                std::string typeLower = attr->attributeType();
                std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(),
                               [](unsigned char ch)
                               {
                                   return static_cast<char>(std::tolower(ch));
                               });

                if (typeLower != "viewcontext")
                {
                    continue;
                }

                std::string currentApsId = aps_identifier_stack.empty() ? std::string() : aps_identifier_stack.back();
                std::string currentApsType = aps_type_stack.empty() ? std::string() : aps_type_stack.back();
                std::string currentApsIdLower = utils::toLower(currentApsId);
                std::string currentApsTypeLower = utils::toLower(currentApsType);
                bool preferContext = (currentApsIdLower == "initview" || currentApsTypeLower == "view");
                if (view_context_count_ > 0 && preferContext && !view_context_multiple_)
                {
                    // Prefer initView/view APS if encountered after a generic viewcontext.
                    view_context_count_ = 0;
                    view_context_active_ = false;
                }
                else if (view_context_count_ > 0 && !preferContext)
                {
                    view_context_multiple_ = true;
                    continue;
                }

                bool parsed = false;
                if (auto rect = SDRParser::parseViewContextRect(attr->data()))
                {
                    parsed = assignViewContextRect(rect->minX, rect->minY, rect->maxX, rect->maxY);
                }

                if (!parsed)
                {
                    if (auto structuredOpt = attr->structuredText())
                    {
                        parsed = parseViewContextTokens(
                            svg::ApsTextDecoder::decodeTokens(*structuredOpt));
                    }
                }

                if (!parsed)
                {
                    parsed = parseViewContextTokens(
                        svg::ApsTextDecoder::decodeTokens(attr->data()));
                }

#ifndef NDEBUG
                if (parsed && std::getenv("SVG_DEBUG_BOUNDS"))
                {
                    std::cerr << "[svg] viewcontext detected rect=[" << view_context_x1_ << "," << view_context_y1_
                              << " -> " << view_context_x2_ << "," << view_context_y2_ << "]\n";
                }
#endif
            }
        }

        const bool haveSingleViewContext = (view_context_active_ && !view_context_multiple_ && view_context_count_ == 1);
        apply_view_context_on_load_ = (conversion_plan_.adopt_view_on_load && haveSingleViewContext);

        const bool allow_autofit = (conversion_plan_.output_profile != OutputProfile::WebCGM21);
        bool fit_to_geometry = false;
        double fit_margin_x = 0.0;
        double fit_margin_y = 0.0;
        double coverage_x = 1.0;
        double coverage_y = 1.0;

        if (allow_autofit && has_points && !apply_view_context_on_load_)
        {
            coverage_x = (picture_width > 0.0) ? (geometry_width / picture_width) : 1.0;
            coverage_y = (picture_height > 0.0) ? (geometry_height / picture_height) : 1.0;
            const double coverage_threshold = 0.2;

            if (coverage_x > 0.0 && coverage_y > 0.0 && (coverage_x < coverage_threshold && coverage_y < coverage_threshold))
            {
                fit_to_geometry = true;
                double margin_fraction = 0.08;
                fit_margin_x = geometry_width * margin_fraction;
                fit_margin_y = geometry_height * margin_fraction;
                double min_margin = std::max(1.0, std::max(geometry_width * 0.02, geometry_height * 0.02));
                if (fit_margin_x < min_margin)
                {
                    fit_margin_x = min_margin;
                }
                if (fit_margin_y < min_margin)
                {
                    fit_margin_y = min_margin;
                }
            }
        }

        if (fit_to_geometry)
        {
            picture_vdc_min_x_ = bounds_x1 - fit_margin_x;
            picture_vdc_max_x_ = bounds_x2 + fit_margin_x;
            picture_vdc_min_y_ = bounds_y1 - fit_margin_y;
            picture_vdc_max_y_ = bounds_y2 + fit_margin_y;
            picture_vdc_width_ = std::abs(picture_vdc_max_x_ - picture_vdc_min_x_);
            picture_vdc_height_ = std::abs(picture_vdc_max_y_ - picture_vdc_min_y_);
        }

        if (picture_vdc_width_ <= 0.0)
        {
            picture_vdc_width_ = 1.0;
        }
        if (picture_vdc_height_ <= 0.0)
        {
            picture_vdc_height_ = 1.0;
        }

        // For WebCGM 2.1 and S1000D, coordinates should remain in VDC space regardless of scaling mode
        // The metric scale factor is for physical measurements, not SVG coordinate space
        // S1000D is WebCGM + hotspot interactivity attributes, so inherits WebCGM coordinate behavior
        double scale_factor = 1.0;
        if (conversion_plan_.output_profile != OutputProfile::WebCGM21 &&
            conversion_plan_.output_profile != OutputProfile::S1000D &&
            conversion_plan_.output_profile != OutputProfile::S1000DLegacy &&
            scaling_mode_ == SpecificationMode::SCALED &&
            metric_scale_factor_ > 0.0)
        {
            scale_factor = metric_scale_factor_;
        }
        svg_canvas_width_ = std::max(picture_vdc_width_ * scale_factor, 1.0);
        svg_canvas_height_ = std::max(picture_vdc_height_ * scale_factor, 1.0);

        // Use original VDC extent preserving axis order (enables proper X-inversion handling)
        transform_.setVdcExtent(picture_vdc_orig_x1_, picture_vdc_orig_y1_, picture_vdc_orig_x2_, picture_vdc_orig_y2_);
        transform_.setSvgSize(svg_canvas_width_, svg_canvas_height_);
        transform_.setFlipY(true);

        double viewbox_x = 0.0;
        double viewbox_y = 0.0;
        double viewbox_width = svg_canvas_width_;
        double viewbox_height = svg_canvas_height_;

        if (apply_view_context_on_load_)
        {
            double vc_min_x = std::min(view_context_x1_, view_context_x2_);
            double vc_max_x = std::max(view_context_x1_, view_context_x2_);
            double vc_min_y = std::min(view_context_y1_, view_context_y2_);
            double vc_max_y = std::max(view_context_y1_, view_context_y2_);

            viewbox_width = std::max((vc_max_x - vc_min_x) * scale_factor, 1e-6);
            viewbox_height = std::max((vc_max_y - vc_min_y) * scale_factor, 1e-6);
            viewbox_x = (vc_min_x - picture_vdc_min_x_) * scale_factor;
            viewbox_y = (picture_vdc_max_y_ - vc_max_y) * scale_factor;

            original_vdc_x1_ = vc_min_x;
            original_vdc_y1_ = vc_min_y;
            original_vdc_x2_ = vc_max_x;
            original_vdc_y2_ = vc_max_y;
        }
        else
        {
            original_vdc_x1_ = picture_vdc_min_x_;
            original_vdc_y1_ = picture_vdc_min_y_;
            original_vdc_x2_ = picture_vdc_max_x_;
            original_vdc_y2_ = picture_vdc_max_y_;
            viewbox_width = svg_canvas_width_;
            viewbox_height = svg_canvas_height_;
            viewbox_x = 0.0;
            viewbox_y = 0.0;
        }

        if (viewbox_padding_fraction_ > 0.0)
        {
            double pad_x = viewbox_width * viewbox_padding_fraction_;
            double pad_y = viewbox_height * viewbox_padding_fraction_;
            viewbox_x -= pad_x;
            viewbox_y -= pad_y;
            viewbox_width += pad_x * 2.0;
            viewbox_height += pad_y * 2.0;
        }

        svg_viewbox_x_ = viewbox_x;
        svg_viewbox_y_ = viewbox_y;
        svg_viewbox_width_ = viewbox_width;
        svg_viewbox_height_ = viewbox_height;

        svg_bounds_x1_ = viewbox_x;
        svg_bounds_y1_ = viewbox_y;
        svg_bounds_x2_ = viewbox_x + viewbox_width;
        svg_bounds_y2_ = viewbox_y + viewbox_height;

#ifndef NDEBUG
        if (const char *debugBounds = std::getenv("SVG_DEBUG_BOUNDS"))
        {
            (void)debugBounds;
            std::cerr << "[svg] viewBox=" << svg_viewbox_x_ << "," << svg_viewbox_y_
                      << " " << svg_viewbox_width_ << "x" << svg_viewbox_height_
                      << (apply_view_context_on_load_ ? " (viewcontext)" : " (picture)") << "\n";
        }
#endif

        double picture_longest_side = std::max(picture_vdc_width_, picture_vdc_height_);
        double bounds_width = std::abs(bounds_x2 - bounds_x1);
        double bounds_height = std::abs(bounds_y2 - bounds_y1);
        double longest_side_reference = picture_longest_side_raw_;
        if (longest_side_reference <= 0.0)
        {
            longest_side_reference = picture_longest_side > 0.0 ? picture_longest_side : std::max(bounds_width, bounds_height);
        }
        if (longest_side_reference <= 0.0)
        {
            longest_side_reference = 1.0;
        }

        const double abstract_divisor = 32768.0;
        abstract_line_width_unit_ = longest_side_reference / abstract_divisor;
        abstract_edge_width_unit_ = longest_side_reference / abstract_divisor;

        auto applyThousandthFallback = [&](double &unit) {
            if (unit <= 0.0 && compatibility_mode_)
            {
                unit = longest_side_reference / 1000.0;
            }
            if (unit <= 0.0)
            {
                unit = 1.0;
            }
        };

        applyThousandthFallback(abstract_line_width_unit_);
        applyThousandthFallback(abstract_edge_width_unit_);

        nominal_line_width_svg_ = std::max(transform_.transformLength(abstract_line_width_unit_), 0.1);
        nominal_edge_width_svg_ = std::max(transform_.transformLength(abstract_edge_width_unit_), 0.1);
        double viewbox_width_final = svg_bounds_x2_ - svg_bounds_x1_;
        double viewbox_height_final = svg_bounds_y2_ - svg_bounds_y1_;
        picture_scale_x_ = (picture_vdc_width_ > 0.0) ? (viewbox_width_final / picture_vdc_width_) : 1.0;
        picture_scale_y_ = (picture_vdc_height_ > 0.0) ? (viewbox_height_final / picture_vdc_height_) : 1.0;

        geometry_metrics_ = GeometryMetrics{};
        geometry_metrics_.has_geometry = has_points;
        geometry_metrics_.geometry_min_x = has_points ? std::min(min_x, max_x) : bounds_x1;
        geometry_metrics_.geometry_min_y = has_points ? std::min(min_y, max_y) : bounds_y1;
        geometry_metrics_.geometry_max_x = has_points ? std::max(min_x, max_x) : bounds_x2;
        geometry_metrics_.geometry_max_y = has_points ? std::max(min_y, max_y) : bounds_y2;
        geometry_metrics_.geometry_width = geometry_width;
        geometry_metrics_.geometry_height = geometry_height;

        geometry_metrics_.picture_min_x = picture_vdc_min_x_;
        geometry_metrics_.picture_min_y = picture_vdc_min_y_;
        geometry_metrics_.picture_max_x = picture_vdc_max_x_;
        geometry_metrics_.picture_max_y = picture_vdc_max_y_;
        geometry_metrics_.picture_width = picture_vdc_width_;
        geometry_metrics_.picture_height = picture_vdc_height_;

        geometry_metrics_.coverage_x = coverage_x;
        geometry_metrics_.coverage_y = coverage_y;
        geometry_metrics_.auto_fit_applied = fit_to_geometry;
        geometry_metrics_.auto_fit_margin_x = fit_margin_x;
        geometry_metrics_.auto_fit_margin_y = fit_margin_y;

        geometry_metrics_.view_context_present = view_context_active_;
        geometry_metrics_.view_context_multiple = view_context_multiple_;
        geometry_metrics_.view_context_adopted = apply_view_context_on_load_;
        geometry_metrics_.view_context_min_x = view_context_x1_;
        geometry_metrics_.view_context_min_y = view_context_y1_;
        geometry_metrics_.view_context_max_x = view_context_x2_;
        geometry_metrics_.view_context_max_y = view_context_y2_;

        geometry_metrics_.canvas_width = svg_canvas_width_;
        geometry_metrics_.canvas_height = svg_canvas_height_;
        geometry_metrics_.scale_factor = scale_factor;
        geometry_metrics_.flip_y_applied = transform_.getFlipY();

        geometry_metrics_.viewbox_x = svg_viewbox_x_;
        geometry_metrics_.viewbox_y = svg_viewbox_y_;
        geometry_metrics_.viewbox_width = svg_viewbox_width_;
        geometry_metrics_.viewbox_height = svg_viewbox_height_;

        geometry_metrics_.compatibility_mode = compatibility_mode_;

        if (geometry_logging_enabled_)
        {
            std::cerr << "[geometry] picture=" << picture_vdc_width_ << "x" << picture_vdc_height_
                      << " bounds=[" << geometry_metrics_.geometry_min_x << "," << geometry_metrics_.geometry_min_y
                      << " -> " << geometry_metrics_.geometry_max_x << "," << geometry_metrics_.geometry_max_y << "]"
                      << " coverage=(" << coverage_x << "," << coverage_y << ")"
                      << (fit_to_geometry ? " autofit" : "")
                      << (apply_view_context_on_load_ ? " viewcontext" : " picture")
                      << (compatibility_mode_ ? " compat" : " strict")
                      << " viewBox=[" << svg_viewbox_x_ << "," << svg_viewbox_y_ << " "
                      << svg_viewbox_width_ << "x" << svg_viewbox_height_ << "]";
            if (view_context_active_)
            {
                std::cerr << " vc=[" << view_context_x1_ << "," << view_context_y1_
                          << " -> " << view_context_x2_ << "," << view_context_y2_ << "]";
                if (view_context_multiple_)
                {
                    std::cerr << " (multiple)";
                }
            }
            std::cerr << " flipY=" << (geometry_metrics_.flip_y_applied ? "yes" : "no") << "\n";
            if (!geometry_metrics_.flip_y_applied)
            {
                std::cerr << "[geometry] warning: transform did not apply Y flip\n";
            }
            if (!has_points)
            {
                std::cerr << "[geometry] warning: picture contains no graphical geometry; defaults used\n";
            }
        }

        document_metrics_dirty_ = false;
    }

    std::string SVGConverter::convert(int pictureIndex)
    {
        conversion_plan_ = pending_conversion_plan_;
        attribute_manager_.setOutputFormat(
            conversion_plan_.attribute_format);
        if (document_metrics_dirty_)
        {
            recomputeDocumentMetrics();
        }

        svg_output_.str("");
        svg_output_.clear();
        encountered_segment_ = false;
        current_picture_index_ = -1;
        raster_metrics_.clear();

        aps_id_allocator_.reset();
        aps_metadata_entries_.clear();
        clip_path_counter_ = 0;
        resetPictureState();

        // Get picture ranges if selective conversion requested
        size_t startIdx = 0;
        size_t endIdx = SIZE_MAX;
        if (pictureIndex >= 0) {
            auto ranges = cgm_file_->getPictureRanges();
            if (pictureIndex < static_cast<int>(ranges.size())) {
                startIdx = ranges[pictureIndex].startCommandIndex;
                endIdx = ranges[pictureIndex].endCommandIndex;
            } else {
                // Invalid picture index - return empty SVG
                writeSvgHeader();
                writeSvgFooter();
                return svg_output_.str();
            }
        }

        writeSvgHeader();

        // Process commands (all or selective range)
        const auto &commands = cgm_file_->commands();
        for (size_t idx = 0; idx < commands.size(); ++idx)
        {
            current_command_index_ = idx;

            // For selective conversion, only process commands in the picture range
            // but always process metafile-level commands (before any picture)
            if (pictureIndex >= 0) {
                bool isMetafileLevel = (idx < startIdx);
                bool isInPictureRange = (idx >= startIdx && idx <= endIdx);
                if (!isMetafileLevel && !isInPictureRange) {
                    continue;  // Skip commands outside the target picture
                }
            }

            processCommand(commands[idx].get());
        }

        flushPendingText();
        writeSvgFooter();

        return svg_output_.str();
    }

    void SVGConverter::writeSvgHeader()
    {
        if (include_document_metadata_ &&
            emit_source_hash_ &&
            !source_hash_computed_ &&
            !source_file_path_.empty())
        {
            source_file_hash_ = utils::computeFileMd5Hex(source_file_path_);
            if (!source_file_hash_.empty())
            {
                std::transform(source_file_hash_.begin(), source_file_hash_.end(),
                               source_file_hash_.begin(),
                               [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
            }
            source_hash_computed_ = true;
        }

        std::string sourceName;
        if (cgm_file_)
        {
            sourceName = utils::trimString(cgm_file_->name());
        }
        if (sourceName.empty() && !source_file_path_.empty())
        {
            sourceName = std::filesystem::path(source_file_path_).filename().string();
        }

        const std::string profileLabel =
            svg::DocumentSerializer::profileLabel(
                conversion_plan_.output_profile,
                requested_profile_label_);
        svg::DocumentHeader document;
        document.viewbox_x = svg_viewbox_x_;
        document.viewbox_y = svg_viewbox_y_;
        document.viewbox_width = svg_viewbox_width_;
        document.viewbox_height = svg_viewbox_height_;
        document.emit_webcgm_namespace =
            conversion_plan_.output_target.emit_webcgm_namespace;
        document.include_metadata = include_document_metadata_;
        document.title = svg::DocumentSerializer::title(
            current_picture_name_,
            sourceName);
        document.description = svg::DocumentSerializer::description(
            opencgm::kSvgqaVersion,
            profileLabel,
            compatibility_mode_,
            segment_policy_ == SegmentPolicy::AllowSegments,
            sourceName,
            source_file_hash_);
        if (output_background_color_)
        {
            document.background_color =
                colorToHexString(*output_background_color_);
        }
        svg_output_ << svg::DocumentSerializer::header(document);
    }

    void SVGConverter::writeSvgFooter()
    {
        closeOpenDefs();
        if (embed_aps_metadata_json_ && !aps_metadata_entries_.empty())
        {
            svg_output_ << svg::DocumentSerializer::apsMetadata(
                aps_metadata_entries_);
        }
        emitViewerShimIfNeeded();
        svg_output_ << svg::DocumentSerializer::close();
    }

    void SVGConverter::emitViewerShimIfNeeded()
    {
        svg_output_ << svg::DocumentSerializer::viewerShim(
            viewer_shim_mode_,
            {has_layer_aps_, has_linkuri_aps_, has_viewcontext_aps_},
            viewer_shim_url_,
            kViewerShimInline);
    }

    void SVGConverter::handleSegmentEncountered(const Command *cmd, const std::string &label)
    {
        encountered_segment_ = true;
        if (segment_policy_ == SegmentPolicy::StrictWebCgm)
        {
            std::ostringstream oss;
            oss << "WebCGM 2.1 Element Set (REC-webcgm21-20100301, Table 2-1) prohibits SEGMENT elements; encountered "
                << (cmd ? cmd->toString() : label)
                << ". Re-run with --allow-segments or --profile=iso8632 to flatten legacy ISO/IEC 8632 segments.";
            throw std::runtime_error(oss.str());
        }
    }

    namespace
    {
        template <typename T, ClassCode ExpectedClass, int ExpectedId>
        T *dispatchCast(Command *cmd)
        {
#ifndef NDEBUG
            if (cmd == nullptr)
            {
                return nullptr;
            }

            if (cmd->elementClass() != ExpectedClass ||
                (ExpectedId >= 0 && cmd->elementId() != ExpectedId))
            {
                std::cerr << "[svg] dispatchCast class/id mismatch: expected class="
                          << static_cast<int>(ExpectedClass)
                          << " id=";
                if (ExpectedId >= 0)
                {
                    std::cerr << ExpectedId;
                }
                else
                {
                    std::cerr << "any";
                }
                std::cerr
                          << ", got class=" << static_cast<int>(cmd->elementClass())
                          << " id=" << cmd->elementId() << "\n";
                return nullptr;
            }

            auto *typed = dynamic_cast<T *>(cmd);
            if (typed == nullptr)
            {
                std::cerr << "[svg] dispatchCast type mismatch for " << cmd->toString() << "\n";
                return nullptr;
            }
            return typed;
#else
            return static_cast<T *>(cmd);
#endif
        }
    } // namespace

    void SVGConverter::processCommand(Command *cmd)
    {
        if (!cmd)
            return;

        ClassCode classCode = cmd->elementClass();
        int elementId = cmd->elementId();
#ifndef NDEBUG
        static const bool debugCommandLogging = []() {
            const char *env = std::getenv("SVG_DEBUG_COMMANDS");
            return (env != nullptr && env[0] != '\0' && env[0] != '0');
        }();
        if (debugCommandLogging)
        {
            std::cerr << "[cmd] idx=" << current_command_index_
                      << " class=" << static_cast<int>(classCode)
                      << " id=" << elementId << "\n";
        }
#endif

        if (classCode == ClassCode::DelimiterElement && elementId == 6)
        {
            handleSegmentEncountered(cmd, "BEGIN SEGMENT");
            return;
        }
        if (classCode == ClassCode::DelimiterElement && elementId == 7)
        {
            handleSegmentEncountered(cmd, "END SEGMENT");
            return;
        }

        if (classCode == ClassCode::SegmentControlandSegmentAttributeElements)
        {
            handleSegmentEncountered(cmd, cmd->toString());
            return;
        }
        if (classCode == ClassCode::MetafileDescriptorElements && elementId == 18)
        {
            handleSegmentEncountered(cmd, cmd->toString());
            return;
        }

        // TEXT=4, RESTRICTED TEXT=5, APPEND TEXT=6
        bool isTextElement = (classCode == ClassCode::GraphicalPrimitiveElements &&
                              (elementId == 4 || elementId == 5));
        bool isAppendText = (classCode == ClassCode::GraphicalPrimitiveElements && elementId == 6);
        // Attribute commands are allowed between TEXT/RESTRICTED TEXT and APPEND TEXT per ISO 8632
        bool isAttributeElement = (classCode == ClassCode::AttributeElements);

        // Only flush pending text if we encounter a non-text, non-attribute command
        if (pending_text_active_ && !(isTextElement || isAppendText || isAttributeElement))
        {
            flushPendingText();
        }
        switch (classCode)
        {
        case ClassCode::GraphicalPrimitiveElements:
            closeOpenDefs();
            switch (elementId)
            {
            case 1: // POLYLINE
                processPolyline(dispatchCast<Polyline, ClassCode::GraphicalPrimitiveElements, 1>(cmd));
                break;
            case 2: // DISJOINT POLYLINE
                processDisjointPolyline(dispatchCast<DisjointPolyline, ClassCode::GraphicalPrimitiveElements, 2>(cmd));
                break;
            case 3: // POLYMARKER
                processPolymarker(dispatchCast<Polymarker, ClassCode::GraphicalPrimitiveElements, 3>(cmd));
                break;
            case 4: // TEXT
                processText(dispatchCast<Text, ClassCode::GraphicalPrimitiveElements, 4>(cmd));
                break;
            case 5: // RESTRICTED TEXT
                processRestrictedText(dispatchCast<RestrictedText, ClassCode::GraphicalPrimitiveElements, 5>(cmd));
                break;
            case 6: // APPEND TEXT
                processAppendText(dispatchCast<AppendText, ClassCode::GraphicalPrimitiveElements, 6>(cmd));
                break;
            case 7: // POLYGON
                processPolygon(dispatchCast<Polygon, ClassCode::GraphicalPrimitiveElements, 7>(cmd));
                break;
            case 8: // POLYGON SET
                processPolygonSet(dispatchCast<PolygonSet, ClassCode::GraphicalPrimitiveElements, 8>(cmd));
                break;
            case 9: // CELL ARRAY
                processCellArray(dispatchCast<CellArray, ClassCode::GraphicalPrimitiveElements, 9>(cmd));
                break;
            case 10: // GENERALIZED DRAWING PRIMITIVE
                processGeneralizedDrawingPrimitive(dispatchCast<GeneralizedDrawingPrimitive, ClassCode::GraphicalPrimitiveElements, 10>(cmd));
                break;
            case 11: // RECTANGLE
                processRectangle(dispatchCast<Rectangle, ClassCode::GraphicalPrimitiveElements, 11>(cmd));
                break;
            case 12: // CIRCLE
                processCircle(dispatchCast<Circle, ClassCode::GraphicalPrimitiveElements, 12>(cmd));
                break;
            case 13: // CIRCULAR ARC 3 POINT
                processCircularArc3Point(dispatchCast<CircularArc3Point, ClassCode::GraphicalPrimitiveElements, 13>(cmd));
                break;
            case 14: // CIRCULAR ARC 3 POINT CLOSE
                processCircularArc3PointClose(dispatchCast<CircularArc3PointClose, ClassCode::GraphicalPrimitiveElements, 14>(cmd));
                break;
            case 15: // CIRCULAR ARC CENTRE
                processCircularArcCentre(dispatchCast<CircularArcCentre, ClassCode::GraphicalPrimitiveElements, 15>(cmd));
                break;
            case 16: // CIRCULAR ARC CENTRE CLOSE
                processCircularArcCentreClose(dispatchCast<CircularArcCentreClose, ClassCode::GraphicalPrimitiveElements, 16>(cmd));
                break;
            case 17: // ELLIPSE
                processEllipse(dispatchCast<Ellipse, ClassCode::GraphicalPrimitiveElements, 17>(cmd));
                break;
            case 18: // ELLIPTICAL ARC
                processEllipticalArc(dispatchCast<EllipticalArc, ClassCode::GraphicalPrimitiveElements, 18>(cmd));
                break;
            case 19: // ELLIPTICAL ARC CLOSE
                processEllipticalArcClose(dispatchCast<EllipticalArcClose, ClassCode::GraphicalPrimitiveElements, 19>(cmd));
                break;
            case 20: // CIRCULAR ARC CENTRE REVERSED
                processCircularArcCentreReversed(dispatchCast<CircularArcCentreReversed, ClassCode::GraphicalPrimitiveElements, 20>(cmd));
                break;
            case 21: // CONNECTING EDGE
                processConnectingEdge(dispatchCast<ConnectingEdge, ClassCode::GraphicalPrimitiveElements, 21>(cmd));
                break;
            case 22: // HYPERBOLIC ARC
                processHyperbolicArc(dispatchCast<HyperbolicArc, ClassCode::GraphicalPrimitiveElements, 22>(cmd));
                break;
            case 23: // PARABOLIC ARC
                processParabolicArc(dispatchCast<ParabolicArc, ClassCode::GraphicalPrimitiveElements, 23>(cmd));
                break;
            case 24: // NON-UNIFORM B-SPLINE
                processNonUniformBSpline(dispatchCast<NonUniformBSpline, ClassCode::GraphicalPrimitiveElements, 24>(cmd));
                break;
            case 25: // NON-UNIFORM RATIONAL B-SPLINE
                processNonUniformRationalBSpline(dispatchCast<NonUniformRationalBSpline, ClassCode::GraphicalPrimitiveElements, 25>(cmd));
                break;
            case 26: // POLYBEZIER
                processPolyBezier(dispatchCast<PolyBezier, ClassCode::GraphicalPrimitiveElements, 26>(cmd));
                break;
            case 27: // POLYSYMBOL
                processPolySymbol(dispatchCast<PolySymbol, ClassCode::GraphicalPrimitiveElements, 27>(cmd));
                break;
            case 28: // BITONAL TILE
                processBitonalTile(dispatchCast<BitonalTile, ClassCode::GraphicalPrimitiveElements, 28>(cmd));
                break;
            case 29: // TILE
                processTile(dispatchCast<Tile, ClassCode::GraphicalPrimitiveElements, 29>(cmd));
                break;
            default:
                break;
            }
            break;

        case ClassCode::MetafileDescriptorElements:
            switch (elementId)
            {
            case 9: // MAXIMUM COLOUR INDEX
                processMaximumColourIndex(dispatchCast<MaximumColourIndex, ClassCode::MetafileDescriptorElements, 9>(cmd));
                break;
            case 10: // COLOUR VALUE EXTENT
                processColourValueExtent(dispatchCast<ColourValueExtent, ClassCode::MetafileDescriptorElements, 10>(cmd));
                break;
            case 13: // FONT LIST
                processFontList(dispatchCast<FontList, ClassCode::MetafileDescriptorElements, 13>(cmd));
                break;
            case 23: // SYMBOL LIBRARY LIST
                processSymbolLibraryList(dispatchCast<SymbolLibraryList, ClassCode::MetafileDescriptorElements, 23>(cmd));
                break;
            default:
                break;
            }
            break;

        case ClassCode::ControlElements:
            switch (elementId)
            {
            case 5: // CLIP RECTANGLE
                processClipRectangle(dispatchCast<ClipRectangle, ClassCode::ControlElements, 5>(cmd));
                break;
            case 6: // CLIP INDICATOR
                processClipIndicator(dispatchCast<ClipIndicator, ClassCode::ControlElements, 6>(cmd));
                break;
            case 17: // PROTECTION REGION INDICATOR
                processProtectionRegionIndicator(dispatchCast<ProtectionRegionIndicator, ClassCode::ControlElements, 17>(cmd));
                break;
            case 19: // MITRE LIMIT
                processMitreLimit(dispatchCast<MitreLimit, ClassCode::ControlElements, 19>(cmd));
                break;
            case 20: // TRANSPARENT CELL COLOUR
                processTransparentCellColour(dispatchCast<TransparentCellColour, ClassCode::ControlElements, 20>(cmd));
                break;
            default:
                break;
            }
            break;

        case ClassCode::AttributeElements:
            switch (elementId)
            {
            case 2: // LINE TYPE
                processLineType(dispatchCast<LineType, ClassCode::AttributeElements, 2>(cmd));
                break;
            case 3: // LINE WIDTH
                processLineWidth(dispatchCast<LineWidth, ClassCode::AttributeElements, 3>(cmd));
                break;
            case 4: // LINE COLOUR
                processLineColor(dispatchCast<LineColour, ClassCode::AttributeElements, 4>(cmd));
                break;
            case 5: // MARKER TYPE
                processMarkerType(dispatchCast<MarkerType, ClassCode::AttributeElements, 5>(cmd));
                break;
            case 6: // MARKER SIZE
                processMarkerSize(dispatchCast<MarkerSize, ClassCode::AttributeElements, 6>(cmd));
                break;
            case 7: // MARKER COLOUR
                processMarkerColor(dispatchCast<MarkerColour, ClassCode::AttributeElements, 7>(cmd));
                break;
            case 10: // TEXT FONT INDEX
                processTextFontIndex(dispatchCast<TextFontIndex, ClassCode::AttributeElements, 10>(cmd));
                break;
            case 11: // TEXT PRECISION
                processTextPrecision(dispatchCast<TextPrecision, ClassCode::AttributeElements, 11>(cmd));
                break;
            case 12: // CHARACTER EXPANSION
                processCharacterExpansion(dispatchCast<CharacterExpansionFactor, ClassCode::AttributeElements, 12>(cmd));
                break;
            case 13: // CHARACTER SPACING
                processCharacterSpacing(dispatchCast<CharacterSpacing, ClassCode::AttributeElements, 13>(cmd));
                break;
            case 14: // TEXT COLOUR
                processTextColor(dispatchCast<TextColour, ClassCode::AttributeElements, 14>(cmd));
                break;
            case 15: // CHARACTER HEIGHT
                processCharacterHeight(dispatchCast<CharacterHeight, ClassCode::AttributeElements, 15>(cmd));
                break;
            case 16: // CHARACTER ORIENTATION
                processCharacterOrientation(dispatchCast<CharacterOrientation, ClassCode::AttributeElements, 16>(cmd));
                break;
            case 17: // TEXT PATH
                processTextPath(cmd);
                break;
            case 18: // TEXT ALIGNMENT
                processTextAlignment(dispatchCast<TextAlignment, ClassCode::AttributeElements, 18>(cmd));
                break;
            case 21: // FILL BUNDLE INDEX
                processFillBundleIndex(dispatchCast<FillBundleIndex, ClassCode::AttributeElements, 21>(cmd));
                break;
            case 22: // INTERIOR STYLE (ISO/IEC 8632-1)
                processInteriorStyle(dispatchCast<InteriorStyle, ClassCode::AttributeElements, 22>(cmd));
                break;
            case 23: // FILL COLOUR (ISO/IEC 8632-1)
                processFillColor(dispatchCast<FillColour, ClassCode::AttributeElements, 23>(cmd));
                break;
            case 24: // HATCH INDEX (ISO/IEC 8632-1)
                processHatchIndex(dispatchCast<HatchIndex, ClassCode::AttributeElements, 24>(cmd));
                break;
            case 25: // PATTERN INDEX
                processPatternIndex(dispatchCast<PatternIndex, ClassCode::AttributeElements, 25>(cmd));
                break;
            case 27: // EDGE TYPE
                processEdgeType(dispatchCast<EdgeType, ClassCode::AttributeElements, 27>(cmd));
                break;
            case 28: // EDGE WIDTH
                processEdgeWidth(dispatchCast<EdgeWidth, ClassCode::AttributeElements, 28>(cmd));
                break;
            case 29: // EDGE COLOUR
                processEdgeColor(dispatchCast<EdgeColour, ClassCode::AttributeElements, 29>(cmd));
                break;
            case 30: // EDGE VISIBILITY
                processEdgeVisibility(dispatchCast<EdgeVisibility, ClassCode::AttributeElements, 30>(cmd));
                break;
            case 31: // FILL REFERENCE POINT
                processFillReferencePoint(dispatchCast<FillReferencePoint, ClassCode::AttributeElements, 31>(cmd));
                break;
            case 32: // PATTERN TABLE
                processPatternTable(dispatchCast<PatternTable, ClassCode::AttributeElements, 32>(cmd));
                break;
            case 33: // PATTERN SIZE (ISO 8632-3: Element 33)
                processPatternSize(dispatchCast<PatternSize, ClassCode::AttributeElements, 33>(cmd));
                break;
            case 34: // COLOUR TABLE (ISO 8632-3: Element 34)
                processColourTable(dispatchCast<ColourTable, ClassCode::AttributeElements, 34>(cmd));
                break;
            case 37: // LINE CAP
                processLineCap(dispatchCast<LineCap, ClassCode::AttributeElements, 37>(cmd));
                break;
            case 38: // LINE JOIN
                processLineJoin(dispatchCast<LineJoin, ClassCode::AttributeElements, 38>(cmd));
                break;
            case 39: // LINE TYPE CONTINUATION
                processLineTypeContinuation(dispatchCast<LineTypeContinuation, ClassCode::AttributeElements, 39>(cmd));
                break;
            case 40: // LINE TYPE INITIAL OFFSET
                processLineTypeInitialOffset(dispatchCast<LineTypeInitialOffset, ClassCode::AttributeElements, 40>(cmd));
                break;
            case 44: // EDGE CAP
                processEdgeCap(dispatchCast<EdgeCap, ClassCode::AttributeElements, 44>(cmd));
                break;
            case 45: // EDGE JOIN
                processEdgeJoin(dispatchCast<EdgeJoin, ClassCode::AttributeElements, 45>(cmd));
                break;
            case 46: // EDGE TYPE CONTINUATION
                processEdgeTypeContinuation(dispatchCast<EdgeTypeContinuation, ClassCode::AttributeElements, 46>(cmd));
                break;
            case 47: // EDGE TYPE INITIAL OFFSET
                processEdgeTypeInitialOffset(dispatchCast<EdgeTypeInitialOffset, ClassCode::AttributeElements, 47>(cmd));
                break;
            }
            break;

        case ClassCode::PictureDescriptorElements:
            switch (elementId)
            {
            case 2: // COLOUR SELECTION MODE
                processColourSelectionMode(dispatchCast<ColourSelectionMode, ClassCode::PictureDescriptorElements, 2>(cmd));
                break;
            case 7: // BACKGROUND COLOUR
            {
                // ISO 8632-1 §6.4.7: BG COLOUR sets the colour-index-0
                // mapping. Mirror the value into color_table_[0] (after
                // CVE) so subsequent COLR-TABLE entries can override it
                // and processBeginPictureBody can paint the canvas with
                // the resolved index 0 value.
                //
                // Skip the color_table_[0] mirror when the precision is
                // > 8 bits AND no explicit CVE has been set — bigcgm04
                // and similar legacy CGMs author "white" as (0xff,0xff,0xff)
                // at 16-bit precision without setting CVE max=(0xff,...).
                // Strict ISO scaling treats those bytes as 255/65535 ≈ 0,
                // producing a near-black BG that would mask foreground
                // primitives. The reference renderers tolerate the
                // authoring quirk; we sidestep it by leaving color_table_[0]
                // at the default-palette white in that case.
                auto *bg = dispatchCast<BackgroundColour, ClassCode::PictureDescriptorElements, 7>(cmd);
                if (bg)
                {
                    Color resolved = applyColourValueExtent(bg->color());
                    background_color_ = resolved;
                    background_color_explicit_ = true;
                    bool cveExplicit =
                        !(colour_value_extent_min_.r == 0 && colour_value_extent_min_.g == 0 && colour_value_extent_min_.b == 0 &&
                          colour_value_extent_max_.r == 255 && colour_value_extent_max_.g == 255 && colour_value_extent_max_.b == 255) &&
                        !(colour_value_extent_min_.r == 0 && colour_value_extent_min_.g == 0 && colour_value_extent_min_.b == 0 &&
                          colour_value_extent_max_.r == 0 && colour_value_extent_max_.g == 0 && colour_value_extent_max_.b == 0);
                    int prec = cgm_file_ ? cgm_file_->colourPrecision() : 8;
                    int maxC = std::max({resolved.r, resolved.g, resolved.b});
                    bool likelyAuthorQuirk = (prec > 8 && !cveExplicit && maxC > 0 && maxC < 50);
                    if (!likelyAuthorQuirk)
                    {
                        color_table_[0] = resolved;
                    }
                }
                break;
            }
            case 14: // FILL REPRESENTATION
                processFillRepresentation(dispatchCast<FillRepresentation, ClassCode::PictureDescriptorElements, 14>(cmd));
                break;
            case 17: // LINE AND EDGE TYPE DEFINITION
                processLineAndEdgeTypeDefinition(dispatchCast<LineAndEdgeTypeDefinition, ClassCode::PictureDescriptorElements, 17>(cmd));
                break;
            case 18: // HATCH STYLE DEFINITION
                processHatchStyleDefinition(dispatchCast<HatchStyleDefinition, ClassCode::PictureDescriptorElements, 18>(cmd));
                break;
            default:
                break;
            }
            break;

        case ClassCode::DelimiterElement:
            switch (elementId)
            {
            case 3: // BEGIN PICTURE
                processBeginPicture(dispatchCast<BeginPicture, ClassCode::DelimiterElement, 3>(cmd));
                break;
            case 4: // BEGIN PICTURE BODY
                processBeginPictureBody(dispatchCast<BeginPictureBody, ClassCode::DelimiterElement, 4>(cmd));
                break;
            case 5: // END PICTURE
                processEndPicture(dispatchCast<EndPicture, ClassCode::DelimiterElement, 5>(cmd));
                break;
            case 13: // BEGIN PROTECTION REGION
                processBeginProtectionRegion(dispatchCast<BeginProtectionRegion, ClassCode::DelimiterElement, 13>(cmd));
                break;
            case 14: // END PROTECTION REGION
                processEndProtectionRegion(dispatchCast<EndProtectionRegion, ClassCode::DelimiterElement, 14>(cmd));
                break;
            case 8: // BEGIN FIGURE
                processBeginFigure(cmd);
                break;
            case 9: // END FIGURE
                processEndFigure(cmd);
                break;
            case 19: // BEGIN TILE ARRAY
                processBeginTileArray(dispatchCast<BeginTileArray, ClassCode::DelimiterElement, 19>(cmd));
                break;
            case 20: // END TILE ARRAY
                processEndTileArray(dispatchCast<EndTileArray, ClassCode::DelimiterElement, 20>(cmd));
                break;
            case 21: // BEGIN APPLICATION STRUCTURE
                processBeginApplicationStructure(dispatchCast<BeginApplicationStructure, ClassCode::DelimiterElement, 21>(cmd));
                break;
            case 22: // BEGIN APPLICATION STRUCTURE BODY
                processBeginApplicationStructureBody(dispatchCast<BeginApplicationStructureBody, ClassCode::DelimiterElement, 22>(cmd));
                break;
            case 23: // END APPLICATION STRUCTURE
                processEndApplicationStructure(dispatchCast<EndApplicationStructure, ClassCode::DelimiterElement, 23>(cmd));
                break;
            }
            break;

        case ClassCode::EscapeElement:
            // Process escape elements (e.g., alpha transparency escape -1)
            processEscape(dispatchCast<Escape, ClassCode::EscapeElement, -1>(cmd));
            break;

        case ClassCode::ApplicationStructureDescriptorElements:
            switch (elementId)
            {
            case 1: // APPLICATION STRUCTURE ATTRIBUTE
                processApplicationStructureAttribute(dispatchCast<ApplicationStructureAttribute, ClassCode::ApplicationStructureDescriptorElements, 1>(cmd));
                break;
            }
            break;

        default:
            break;
        }
    }

    void SVGConverter::processPolyline(Polyline *cmd)
    {
        if (!cmd || cmd->points().empty())
            return;

        // If we're in a figure, accumulate the polyline points
        if (in_figure_)
        {
            std::vector<CGMPoint> transformed_points;
            for (const auto &pt : cmd->points())
            {
                transformed_points.push_back(transform_.transformPoint(pt));
            }
            figure_polylines_.push_back(transformed_points);
            if (!transformed_points.empty())
            {
                std::ostringstream s;
                s.setf(std::ios::fixed);
                s << "M " << transformed_points[0].x() << " " << transformed_points[0].y();
                for (size_t i = 1; i < transformed_points.size(); ++i)
                    s << " L " << transformed_points[i].x() << " " << transformed_points[i].y();
                s << " ";
                figure_ordered_subpaths_.push_back({s.str(), transformed_points.front(), transformed_points.back(), pending_connect_to_prev_});
                pending_connect_to_prev_ = false;
            }
            return;
        }

        if (capturingProtectionRegionClip())
        {
            // Treat polylines as the boundary of a closed region. SVG clipPath
            // requires filled shapes; convert M/L sequence to a closed path.
            std::ostringstream pathStream;
            pathStream.setf(std::ios::fixed);
            const auto &pts = cmd->points();
            CGMPoint first = transform_.transformPoint(pts.front());
            pathStream << "M " << first.x() << " " << first.y();
            for (size_t i = 1; i < pts.size(); ++i)
            {
                CGMPoint sp = transform_.transformPoint(pts[i]);
                pathStream << " L " << sp.x() << " " << sp.y();
            }
            pathStream << " Z";
            protection_region_paths_.push_back(pathStream.str());
            return;
        }

        svg_output_ << "  <polyline points=\"";

        bool first = true;
        for (const auto &pt : cmd->points())
        {
            if (!first)
                svg_output_ << " ";
            first = false;

            CGMPoint svg_pt = transform_.transformPoint(pt);
            svg_output_ << svg_pt.x() << "," << svg_pt.y();
        }

        svg_output_ << "\" " << clipPathAttribute() << current_style_.getStrokeStyle()
                    << "fill=\"none\" />\n";
    }

    void SVGConverter::processDisjointPolyline(DisjointPolyline *cmd)
    {
        if (!cmd)
            return;

        const auto &points = cmd->points();
        if (points.size() < 2)
        {
            return;
        }

        if (in_figure_)
        {
            for (size_t i = 0; i + 1 < points.size(); i += 2)
            {
                CGMPoint a = transform_.transformPoint(points[i]);
                CGMPoint b = transform_.transformPoint(points[i + 1]);
                std::vector<CGMPoint> segment{a, b};
                figure_polylines_.push_back(std::move(segment));
                std::ostringstream s;
                s.setf(std::ios::fixed);
                s << "M " << a.x() << " " << a.y() << " L " << b.x() << " " << b.y() << " ";
                figure_ordered_subpaths_.push_back({s.str(), a, b, pending_connect_to_prev_});
                pending_connect_to_prev_ = false;
            }
            return;
        }

        int fillStyle = current_style_.fillStyle();
        bool edgeVis = current_style_.edgeVisibility();

        if (debug_fill_logging_)
        {
            const Color &fc = current_style_.fillColor();
            std::cerr << "[svg] figure cmd=" << current_command_index_
                      << " fillStyle=" << fillStyle
                      << " edgeVis=" << (edgeVis ? 1 : 0)
                      << " fillColor=" << static_cast<int>(fc.r) << ","
                      << static_cast<int>(fc.g) << ","
                      << static_cast<int>(fc.b)
                      << " polygons=" << figure_polylines_.size() << "\n";
        }

        svg_output_ << "  <path d=\"";
        bool firstSegment = true;

        for (size_t i = 0; i + 1 < points.size(); i += 2)
        {
            CGMPoint start = transform_.transformPoint(points[i]);
            CGMPoint end = transform_.transformPoint(points[i + 1]);

            if (!firstSegment)
            {
                svg_output_ << " ";
            }

            svg_output_ << "M " << start.x() << " " << start.y()
                        << " L " << end.x() << " " << end.y();
            firstSegment = false;
        }

        svg_output_ << "\" " << clipPathAttribute() << current_style_.getStrokeStyle()
                    << "fill=\"none\" />\n";
    }

    void SVGConverter::processPolygon(Polygon *cmd)
    {
        if (!cmd || cmd->points().empty())
            return;

        const auto &points = cmd->points();
        if (debug_fill_logging_)
        {
            const Color &fc = current_style_.fillColor();
            std::cerr << "[svg] polygon cmd=" << current_command_index_
                      << " interior=" << interiorStyleName(current_style_.fillStyle())
                      << " edgeVis=" << (current_style_.edgeVisibility() ? 1 : 0)
                      << " fillColor=" << static_cast<int>(fc.r) << ","
                      << static_cast<int>(fc.g) << ","
                      << static_cast<int>(fc.b) << "\n";
        }

        std::ostringstream path;
        path.setf(std::ios::fixed);
        if (!points.empty())
        {
            CGMPoint first = transform_.transformPoint(points.front());
            path << "M " << first.x() << " " << first.y();

            for (size_t i = 1; i < points.size(); ++i)
            {
                CGMPoint svg_pt = transform_.transformPoint(points[i]);
                path << " L " << svg_pt.x() << " " << svg_pt.y();
            }
            path << " Z";
        }

        if (in_figure_)
        {
            // Inside FIGURE: capture as a closed Z-terminated subpath so the
            // polygon participates in the consolidated figure path. FIGURE06
            // (outer pentagon + 5 inner pentagons in one BEGIN FIGURE block)
            // depends on this — only when all polygons share one path with
            // evenodd fill do the inner pentagons render as holes.
            std::string frag = path.str() + " ";
            CGMPoint first = transform_.transformPoint(points.front());
            figure_path_fragments_.push_back(frag);
            figure_ordered_subpaths_.push_back({frag, first, first, pending_connect_to_prev_});
            pending_connect_to_prev_ = false;
            return;
        }

        if (capturingProtectionRegionClip())
        {
            protection_region_paths_.push_back(path.str());
            return;
        }

        std::string styleAttributes = buildFillAndEdgeAttributes();
        closeOpenDefs();
        svg_output_ << "  <path d=\"" << path.str() << "\" "
                    << clipPathAttribute() << debugCommandAttribute()
                    << "fill-rule=\"evenodd\" " << styleAttributes << "/>\n";
    }

    void SVGConverter::processPolygonSet(PolygonSet *cmd)
    {
        if (!cmd || cmd->edges().empty())
            return;

        const auto &edges = cmd->edges();

        std::vector<std::vector<CGMPoint>> polygons;
        std::vector<bool> polygonVisible;
        std::vector<CGMPoint> currentPolygon;
        bool currentHasVisibleEdge = false;

        auto flushPolygon = [&]()
        {
            if (currentPolygon.size() >= 3)
            {
                polygons.push_back(currentPolygon);
                polygonVisible.push_back(currentHasVisibleEdge);
            }
            currentPolygon.clear();
            currentHasVisibleEdge = false;
        };

        for (size_t i = 0; i < edges.size(); ++i)
        {
            const auto &edge = edges[i];
            currentPolygon.push_back(edge.point);

            if (edge.edgeOutFlag == 1 || edge.edgeOutFlag == 3)
            {
                currentHasVisibleEdge = true;
            }

            bool closesPolygon = (edge.edgeOutFlag == 2 || edge.edgeOutFlag == 3);
            if (closesPolygon)
            {
                flushPolygon();
            }
        }
        if (!currentPolygon.empty())
        {
            flushPolygon();
        }

        if (polygons.empty())
        {
            return;
        }

        bool anyVisibleEdges = false;
        for (bool visible : polygonVisible)
        {
            if (visible)
            {
                anyVisibleEdges = true;
                break;
            }
        }

        std::ostringstream pathData;
        pathData.setf(std::ios::fixed);
        pathData << std::setprecision(3);

        for (const auto &polygon : polygons)
        {
            if (polygon.size() < 3)
            {
                continue;
            }

            CGMPoint first = transform_.transformPoint(polygon.front());
            pathData << "M " << first.x() << " " << first.y();

            for (size_t i = 1; i < polygon.size(); ++i)
            {
                CGMPoint pt = transform_.transformPoint(polygon[i]);
                pathData << " L " << pt.x() << " " << pt.y();
            }

            pathData << " Z ";
        }

        std::string data = pathData.str();
        if (data.empty())
        {
            return;
        }

        auto signedArea = [](const std::vector<CGMPoint> &ring) -> double
        {
            if (ring.size() < 3)
            {
                return 0.0;
            }
            double area = 0.0;
            for (size_t i = 0; i < ring.size(); ++i)
            {
                const auto &p = ring[i];
                const auto &q = ring[(i + 1) % ring.size()];
                area += (p.x() * q.y()) - (q.x() * p.y());
            }
            return 0.5 * area;
        };

        int positiveOrientation = 0;
        int negativeOrientation = 0;
        for (const auto &ring : polygons)
        {
            double area = signedArea(ring);
            if (area > 0.0)
            {
                ++positiveOrientation;
            }
            else if (area < 0.0)
            {
                ++negativeOrientation;
            }
        }

        if (debug_fill_logging_)
        {
            const Color &fc = current_style_.fillColor();
            std::cerr << "[svg] polygon-set cmd=" << current_command_index_
                      << " interior=" << interiorStyleName(current_style_.fillStyle())
                      << " edgeVis=" << (current_style_.edgeVisibility() ? 1 : 0)
                      << " polygons=" << polygons.size()
                      << " fillColor=" << static_cast<int>(fc.r) << ","
                      << static_cast<int>(fc.g) << ","
                      << static_cast<int>(fc.b) << "\n";
        }

        bool edgeVisible = anyVisibleEdges && current_style_.edgeVisibility();
        std::string styleAttributes = buildFillAndEdgeAttributes(true, edgeVisible);

        closeOpenDefs();

        svg_output_ << "  <path d=\"" << data << "\" fill-rule=\"evenodd\" "
                    << clipPathAttribute() << debugCommandAttribute()
                    << styleAttributes << "/>\n";
    }

    void SVGConverter::processCircle(Circle *cmd)
    {
        if (!cmd)
            return;

        CGMPoint center_svg = transform_.transformPoint(cmd->center());
        double scale_x = std::fabs(transform_.scaleX());
        double scale_y = std::fabs(transform_.scaleY());
        double rx = cmd->radius() * scale_x;
        double ry = cmd->radius() * scale_y;

        if (in_figure_)
        {
            // Inside BEGIN FIGURE / END FIGURE: capture as a closed subpath
            // (two large arcs trace the full circumference, ending with Z)
            // so renderFigure can include it in the consolidated fill path.
            // Without this branch, circle inside figure emits a standalone
            // <circle> with its own fill+stroke that overlaps the figure's
            // consolidated path (FIGURE07 mixes RECT/CIRCLE/ELLIPSE with
            // POLYLINE/POLYGON/ARCs in one figure block).
            std::ostringstream frag;
            frag.setf(std::ios::fixed);
            double cx = center_svg.x();
            double cy = center_svg.y();
            frag << "M " << (cx - rx) << " " << cy
                 << " A " << rx << " " << ry << " 0 1 0 " << (cx + rx) << " " << cy
                 << " A " << rx << " " << ry << " 0 1 0 " << (cx - rx) << " " << cy
                 << " Z ";
            CGMPoint sp(cx - rx, cy);
            CGMPoint ep(cx - rx, cy);
            figure_path_fragments_.push_back(frag.str());
            figure_ordered_subpaths_.push_back({frag.str(), sp, ep, pending_connect_to_prev_});
            pending_connect_to_prev_ = false;
            return;
        }

        double tol = std::max({1.0, std::fabs(rx), std::fabs(ry)}) * 1e-6;
        bool isotropic = std::fabs(rx - ry) <= tol;

        if (capturingProtectionRegionClip())
        {
            // Approximate the circle as an SVG arc-based path so it fits inside
            // a <clipPath>'s <path d="..."> shape. Two large arcs trace the
            // full circumference; the path is closed for fill-based clipping.
            std::ostringstream pathStream;
            pathStream.setf(std::ios::fixed);
            double cx = center_svg.x();
            double cy = center_svg.y();
            pathStream << "M " << (cx - rx) << " " << cy
                       << " A " << rx << " " << ry << " 0 1 0 " << (cx + rx) << " " << cy
                       << " A " << rx << " " << ry << " 0 1 0 " << (cx - rx) << " " << cy
                       << " Z";
            protection_region_paths_.push_back(pathStream.str());
            return;
        }

        int fillStyle = current_style_.fillStyle();
        bool edgeVis = current_style_.edgeVisibility();

        // In compatibility mode, for unfilled shapes (HOLLOW=0 or EMPTY=4), assume edges
        // should be visible since otherwise the shape would be completely invisible
        bool effectiveEdgeVis = edgeVis;
        if (compatibility_mode_ && !edgeVis && (fillStyle == 0 || fillStyle == 4))
        {
            effectiveEdgeVis = true;
            // Temporarily enable edge visibility so getEdgeStyle() returns proper stroke
            current_style_.setEdgeVisibility(true);
        }

        std::string fillAttr;
        if (fillStyle == 1 || fillStyle == 2 || fillStyle == 3)
        {
            fillAttr = getFillAttributeForCurrentStyle();
            closeOpenDefs();
        }

        if (isotropic)
        {
            double r = 0.5 * (rx + ry);
            svg_output_ << "  <circle cx=\"" << center_svg.x()
                        << "\" cy=\"" << center_svg.y()
                        << "\" r=\"" << r << "\" ";
        }
        else
        {
            svg_output_ << "  <ellipse cx=\"" << center_svg.x()
                        << "\" cy=\"" << center_svg.y()
                        << "\" rx=\"" << rx
                        << "\" ry=\"" << ry << "\" ";
        }

        svg_output_ << clipPathAttribute();

        if (fillStyle == 1 || fillStyle == 2 || fillStyle == 3)
        {
            if (effectiveEdgeVis)
            {
                svg_output_ << fillAttr << current_style_.getEdgeStyle() << "/>\n";
            }
            else
            {
                svg_output_ << fillAttr << "stroke=\"none\" />\n";
            }
        }
        else
        {
            if (effectiveEdgeVis)
            {
                svg_output_ << current_style_.getEdgeStyle() << "fill=\"none\" />\n";
            }
            else
            {
                svg_output_ << "fill=\"none\" stroke=\"none\" />\n";
            }
        }

        // Restore original edge visibility if we changed it
        if (effectiveEdgeVis && !edgeVis)
        {
            current_style_.setEdgeVisibility(false);
        }
    }

    void SVGConverter::processEllipse(Ellipse *cmd)
    {
        if (!cmd)
            return;

        CGMPoint center_svg = transform_.transformPoint(cmd->center());
        CGMPoint cdp1_svg = transform_.transformPoint(cmd->firstConjugateDiameter());
        CGMPoint cdp2_svg = transform_.transformPoint(cmd->secondConjugateDiameter());

        // Convert conjugate diameters to true ellipse parameters (rx, ry, rotation)
        CGMPoint u_svg(cdp1_svg.x() - center_svg.x(), cdp1_svg.y() - center_svg.y());
        CGMPoint v_svg(cdp2_svg.x() - center_svg.x(), cdp2_svg.y() - center_svg.y());
        auto [rx, ry, angle] = conjugateDiametersToEllipse(u_svg, v_svg);

        if (in_figure_)
        {
            // Inside FIGURE: emit as a closed path subpath. SVG <ellipse>
            // can't be embedded in a path's d-attribute directly, so we
            // approximate the ellipse with two large arcs.
            std::ostringstream frag;
            frag.setf(std::ios::fixed);
            double cx = center_svg.x();
            double cy = center_svg.y();
            // Build path data with rotation applied to the arc x-axis
            frag << "M " << (cx - rx) << " " << cy
                 << " A " << rx << " " << ry << " " << angle << " 1 0 " << (cx + rx) << " " << cy
                 << " A " << rx << " " << ry << " " << angle << " 1 0 " << (cx - rx) << " " << cy
                 << " Z ";
            CGMPoint sp(cx - rx, cy);
            CGMPoint ep(cx - rx, cy);
            figure_path_fragments_.push_back(frag.str());
            figure_ordered_subpaths_.push_back({frag.str(), sp, ep, pending_connect_to_prev_});
            pending_connect_to_prev_ = false;
            return;
        }

        int fillStyle = current_style_.fillStyle();
        bool edgeVis = current_style_.edgeVisibility();
        std::string fillAttr;
        if (fillStyle == 1 || fillStyle == 2 || fillStyle == 3)
        {
            fillAttr = getFillAttributeForCurrentStyle();
            closeOpenDefs();
        }

        svg_output_ << "  <ellipse cx=\"" << center_svg.x()
                    << "\" cy=\"" << center_svg.y()
                    << "\" rx=\"" << rx
                    << "\" ry=\"" << ry << "\" ";

        if (std::abs(angle) > 0.01)
        {
            svg_output_ << "transform=\"rotate(" << angle << " "
                        << center_svg.x() << " " << center_svg.y() << ")\" ";
        }
        svg_output_ << clipPathAttribute();

        // Ellipses are filled area primitives in CGM, so they use edge attributes (not line attributes)
        // Standard CGM rendering per specification:
        // - SOLID (1) or HATCH (3): Render with fill
        // - HOLLOW (0), EMPTY (4): Render without fill
        // - EdgeVisibility determines whether to show stroke
        if (fillStyle == 1 || fillStyle == 2 || fillStyle == 3)
        {
            // SOLID or HATCH - render as filled
            if (edgeVis)
            {
                // Fill with stroke
                svg_output_ << fillAttr << current_style_.getEdgeStyle() << "/>\n";
            }
            else
            {
                // Fill without stroke
                svg_output_ << fillAttr << "stroke=\"none\" />\n";
            }
        }
        else
        {
            // HOLLOW, EMPTY, or others - render without fill
            if (edgeVis)
            {
                // Stroke only
                svg_output_ << current_style_.getEdgeStyle() << "fill=\"none\" />\n";
            }
            else
            {
                // No fill, no stroke - transparent (rare but valid)
                svg_output_ << "fill=\"none\" stroke=\"none\" />\n";
            }
        }
    }

    void SVGConverter::processEllipticalArc(EllipticalArc *cmd)
    {
        if (!cmd)
            return;

        CGMPoint center_vdc = cmd->center();
        CGMPoint cdp1_vdc = cmd->firstConjugate();
        CGMPoint cdp2_vdc = cmd->secondConjugate();

        CGMPoint center_svg = transform_.transformPoint(center_vdc);
        CGMPoint cdp1_svg = transform_.transformPoint(cdp1_vdc);
        CGMPoint cdp2_svg = transform_.transformPoint(cdp2_vdc);

        // Convert conjugate diameters to true ellipse parameters (rx, ry, rotation)
        CGMPoint u_svg(cdp1_svg.x() - center_svg.x(), cdp1_svg.y() - center_svg.y());
        CGMPoint v_svg(cdp2_svg.x() - center_svg.x(), cdp2_svg.y() - center_svg.y());
        auto [rx, ry, rotation] = conjugateDiametersToEllipse(u_svg, v_svg);

        CGMPoint startDelta = cmd->startDelta();
        CGMPoint endDelta = cmd->endDelta();

        if (vectorsNearlyEqual(startDelta, endDelta))
        {
            svg_output_ << "  <ellipse cx=\"" << center_svg.x()
                        << "\" cy=\"" << center_svg.y()
                        << "\" rx=\"" << rx << "\" ry=\"" << ry << "\" ";
            if (std::abs(rotation) > 0.01)
            {
                svg_output_ << "transform=\"rotate(" << rotation << " "
                            << center_svg.x() << " " << center_svg.y() << ")\" ";
            }
            svg_output_ << current_style_.getStrokeStyle() << "fill=\"none\" />\n";
            return;
        }
        // Compute parametric angles in ellipse basis (VDC space)
        CGMPoint u_vdc(cdp1_vdc.x() - center_vdc.x(), cdp1_vdc.y() - center_vdc.y());
        CGMPoint v_vdc(cdp2_vdc.x() - center_vdc.x(), cdp2_vdc.y() - center_vdc.y());
        auto scaleToEllipse = [&](const CGMPoint &delta) -> CGMPoint
        {
            const double ux = u_vdc.x();
            const double uy = u_vdc.y();
            const double vx = v_vdc.x();
            const double vy = v_vdc.y();
            double det = ux * vy - uy * vx;
            const double eps = 1e-12;
            if (std::fabs(det) < eps)
            {
                return delta;
            }
            const double inv00 =  vy / det;
            const double inv01 = -vx / det;
            const double inv10 = -uy / det;
            const double inv11 =  ux / det;

            const double q00 = inv00 * inv00 + inv10 * inv10;
            const double q01 = inv00 * inv01 + inv10 * inv11;
            const double q11 = inv01 * inv01 + inv11 * inv11;

            const double dx = delta.x();
            const double dy = delta.y();
            double denom = dx * (q00 * dx + q01 * dy) + dy * (q01 * dx + q11 * dy);
            if (denom <= eps)
            {
                return delta;
            }
            double scale = 1.0 / std::sqrt(denom);
            return CGMPoint(delta.x() * scale, delta.y() * scale);
        };

        CGMPoint startScaled = scaleToEllipse(startDelta);
        CGMPoint endScaled = scaleToEllipse(endDelta);

        CGMPoint start_vdc(center_vdc.x() + startScaled.x(),
                           center_vdc.y() + startScaled.y());
        CGMPoint end_vdc(center_vdc.x() + endScaled.x(),
                         center_vdc.y() + endScaled.y());

        CGMPoint start_svg = transform_.transformPoint(start_vdc);
        CGMPoint end_svg = transform_.transformPoint(end_vdc);

        // Check VDC axis inversion
        bool x_inv = picture_vdc_x_left_;
        bool y_inv = picture_vdc_y_down_;

        // Compute cross product of start/end delta vectors (used in fallback path)
        double cross = startDelta.x() * endDelta.y() - startDelta.y() * endDelta.x();

        double tStart = 0.0, tEnd = 0.0, detM = 0.0;
        bool haveParam = ellipseAnglesFromDeltas(u_vdc, v_vdc, startDelta, endDelta, tStart, tEnd, detM);

        // Per CGM spec ISO/IEC 8632-1:1999 Section 7.6.18:
        // "The drawing direction of the elliptical arc is the direction from the first
        // conjugate radius to the second conjugate radius through the smaller of these two angles."
        // The arc always traverses in the direction of increasing parametric t.
        // sign(detM) determines whether increasing t is CCW (detM > 0) or CW (detM < 0) in VDC.
        double span;
        bool arc_is_ccw;

        if (haveParam)
        {
            span = angularDistanceCCW(tStart, tEnd);
            if (span < kAngleTolerance)
            {
                span = 2.0 * M_PI;
            }
            // Arc direction is fully determined by the sign of the basis determinant.
            arc_is_ccw = (detM > 0);
        }
        else
        {
            // Fallback: ellipse basis is degenerate; approximate using raw VDC angles.
            // Always draw the shorter arc (determined by cross product sign).
            bool cross_ccw = (cross >= 0);
            double start_angle_vdc = std::atan2(startDelta.y(), startDelta.x());
            double end_angle_vdc = std::atan2(endDelta.y(), endDelta.x());
            double ccw_span = angularDistanceCCW(start_angle_vdc, end_angle_vdc);
            span = cross_ccw ? ccw_span : (2.0 * M_PI - ccw_span);
            if (span < kAngleTolerance)
            {
                span = 2.0 * M_PI;
            }
            arc_is_ccw = cross_ccw;
        }

        // Convert VDC arc direction to SVG sweep flag.
        // SVG sweep-flag: 0 = CW (screen coords, Y-down), 1 = CCW.
        // For Y-up VDC: CCW in VDC maps to CW in SVG → sweep_flag = 0.
        // For Y-down VDC: CCW in VDC maps to CCW in SVG → sweep_flag = 1.
        // X-inversion flips CW/CCW perception.
        int sweep_flag;
        bool vdc_y_up = !y_inv;
        if (vdc_y_up) {
            sweep_flag = arc_is_ccw ? 0 : 1;
        } else {
            sweep_flag = arc_is_ccw ? 1 : 0;
        }
        if (x_inv) {
            sweep_flag = 1 - sweep_flag;
        }

        // large_arc: 1 if the arc spans more than half the ellipse (parametric span > π).
        int large_arc = (span > M_PI + kAngleTolerance) ? 1 : 0;

        if (in_figure_)
        {
            std::ostringstream frag;
            frag << "M " << start_svg.x() << " " << start_svg.y()
                 << " A " << rx << " " << ry << " " << rotation << " "
                 << large_arc << " " << sweep_flag << " "
                 << end_svg.x() << " " << end_svg.y() << " ";
            figure_path_fragments_.push_back(frag.str());
            figure_ordered_subpaths_.push_back({frag.str(), start_svg, end_svg, pending_connect_to_prev_});
            pending_connect_to_prev_ = false;
            return;
        }

        svg_output_ << "  <path d=\"M " << start_svg.x() << " " << start_svg.y()
                    << " A " << rx << " " << ry << " " << rotation << " "
                    << large_arc << " " << sweep_flag << " "
                    << end_svg.x() << " " << end_svg.y()
                    << "\" " << current_style_.getStrokeStyle() << "fill=\"none\" />\n";
    }

    void SVGConverter::processEllipticalArcClose(EllipticalArcClose *cmd)
    {
        if (!cmd)
            return;

        CGMPoint center_vdc = cmd->center();
        CGMPoint cdp1_vdc = cmd->firstConjugate();
        CGMPoint cdp2_vdc = cmd->secondConjugate();

        CGMPoint center_svg = transform_.transformPoint(center_vdc);
        CGMPoint cdp1_svg = transform_.transformPoint(cdp1_vdc);
        CGMPoint cdp2_svg = transform_.transformPoint(cdp2_vdc);

        CGMPoint startDelta = cmd->startDelta();
        CGMPoint endDelta = cmd->endDelta();

        // Convert conjugate diameters to true ellipse parameters (rx, ry, rotation)
        CGMPoint u_svg(cdp1_svg.x() - center_svg.x(), cdp1_svg.y() - center_svg.y());
        CGMPoint v_svg(cdp2_svg.x() - center_svg.x(), cdp2_svg.y() - center_svg.y());
        auto [rx, ry, rotation] = conjugateDiametersToEllipse(u_svg, v_svg);

        if (rx <= 1e-6 || ry <= 1e-6)
        {
            CGMPoint start_vdc(center_vdc.x() + startDelta.x(),
                               center_vdc.y() + startDelta.y());
            CGMPoint end_vdc(center_vdc.x() + endDelta.x(),
                             center_vdc.y() + endDelta.y());
            CGMPoint start_svg = transform_.transformPoint(start_vdc);
            CGMPoint end_svg = transform_.transformPoint(end_vdc);
            svg_output_ << "  <path d=\"M " << start_svg.x() << " " << start_svg.y()
                        << " L " << end_svg.x() << " " << end_svg.y();
            if (cmd->closure() == 0)
            {
                svg_output_ << " L " << center_svg.x() << " " << center_svg.y() << " Z";
            }
            else
            {
                svg_output_ << " Z";
            }
            svg_output_ << "\" " << current_style_.getStyleWithEdges() << "/>\n";
            return;
        }

        CGMPoint u_vdc(cdp1_vdc.x() - center_vdc.x(), cdp1_vdc.y() - center_vdc.y());
        CGMPoint v_vdc(cdp2_vdc.x() - center_vdc.x(), cdp2_vdc.y() - center_vdc.y());
        auto scaleToEllipse = [&](const CGMPoint &delta) -> CGMPoint
        {
            const double ux = u_vdc.x();
            const double uy = u_vdc.y();
            const double vx = v_vdc.x();
            const double vy = v_vdc.y();
            double det = ux * vy - uy * vx;
            const double eps = 1e-12;
            if (std::fabs(det) < eps)
            {
                return delta;
            }
            const double inv00 =  vy / det;
            const double inv01 = -vx / det;
            const double inv10 = -uy / det;
            const double inv11 =  ux / det;

            const double q00 = inv00 * inv00 + inv10 * inv10;
            const double q01 = inv00 * inv01 + inv10 * inv11;
            const double q11 = inv01 * inv01 + inv11 * inv11;

            const double dx = delta.x();
            const double dy = delta.y();
            double denom = dx * (q00 * dx + q01 * dy) + dy * (q01 * dx + q11 * dy);
            if (denom <= eps)
            {
                return delta;
            }
            double scale = 1.0 / std::sqrt(denom);
            return CGMPoint(delta.x() * scale, delta.y() * scale);
        };

        CGMPoint startScaled = scaleToEllipse(startDelta);
        CGMPoint endScaled = scaleToEllipse(endDelta);

        CGMPoint start_vdc(center_vdc.x() + startScaled.x(),
                           center_vdc.y() + startScaled.y());
        CGMPoint end_vdc(center_vdc.x() + endScaled.x(),
                         center_vdc.y() + endScaled.y());

        CGMPoint start_svg = transform_.transformPoint(start_vdc);
        CGMPoint end_svg = transform_.transformPoint(end_vdc);

        if (vectorsNearlyEqual(startDelta, endDelta))
        {
            svg_output_ << "  <ellipse cx=\"" << center_svg.x()
                        << "\" cy=\"" << center_svg.y()
                        << "\" rx=\"" << rx << "\" ry=\"" << ry << "\" ";
            svg_output_ << current_style_.getStyleWithEdges();
            svg_output_ << "/>\n";
            return;
        }

        // Check VDC axis inversion
        bool x_inv = picture_vdc_x_left_;
        bool y_inv = picture_vdc_y_down_;

        // Compute cross product of start/end delta vectors (used in fallback path)
        double cross = startDelta.x() * endDelta.y() - startDelta.y() * endDelta.x();

        // Compute parametric angles in ellipse basis (VDC space)
        double tStart = 0.0, tEnd = 0.0, detM = 0.0;
        bool haveParam = ellipseAnglesFromDeltas(u_vdc, v_vdc, startDelta, endDelta, tStart, tEnd, detM);

        // Per CGM spec ISO/IEC 8632-1:1999 Section 7.6.18:
        // "The drawing direction of the elliptical arc is the direction from the first
        // conjugate radius to the second conjugate radius through the smaller of these two angles."
        // The arc always traverses in the direction of increasing parametric t.
        // sign(detM) determines whether increasing t is CCW (detM > 0) or CW (detM < 0) in VDC.
        double span;
        bool arc_is_ccw;

        if (haveParam)
        {
            span = angularDistanceCCW(tStart, tEnd);
            if (span < kAngleTolerance)
            {
                span = 2.0 * M_PI;
            }
            // Arc direction is fully determined by the sign of the basis determinant.
            arc_is_ccw = (detM > 0);
        }
        else
        {
            // Fallback: ellipse basis is degenerate; approximate using raw VDC angles.
            // Always draw the shorter arc (determined by cross product sign).
            bool cross_ccw = (cross >= 0);
            double start_angle_vdc = std::atan2(startDelta.y(), startDelta.x());
            double end_angle_vdc = std::atan2(endDelta.y(), endDelta.x());
            double ccw_span = angularDistanceCCW(start_angle_vdc, end_angle_vdc);
            span = cross_ccw ? ccw_span : (2.0 * M_PI - ccw_span);
            if (span < kAngleTolerance)
            {
                span = 2.0 * M_PI;
            }
            arc_is_ccw = cross_ccw;
        }

        // Convert VDC arc direction to SVG sweep flag.
        // SVG sweep-flag: 0 = CW (screen coords, Y-down), 1 = CCW.
        // For Y-up VDC: CCW in VDC maps to CW in SVG → sweep_flag = 0.
        // For Y-down VDC: CCW in VDC maps to CCW in SVG → sweep_flag = 1.
        // X-inversion flips CW/CCW perception.
        int sweep_flag;
        bool vdc_y_up = !y_inv;
        if (vdc_y_up) {
            sweep_flag = arc_is_ccw ? 0 : 1;
        } else {
            sweep_flag = arc_is_ccw ? 1 : 0;
        }
        if (x_inv) {
            sweep_flag = 1 - sweep_flag;
        }

        // large_arc: 1 if the arc spans more than half the ellipse (parametric span > π).
        int large_arc = (span > M_PI + kAngleTolerance) ? 1 : 0;

        std::string fillAttr = getFillAttributeForCurrentStyle();
        closeOpenDefs();

        svg_output_ << "  <path d=\"M " << start_svg.x() << " " << start_svg.y()
                    << " A " << rx << " " << ry << " " << rotation << " "
                    << large_arc << " " << sweep_flag << " "
                    << end_svg.x() << " " << end_svg.y();

        if (cmd->closure() == 0)
        {
            svg_output_ << " L " << center_svg.x() << " " << center_svg.y() << " Z";
        }
        else
        {
            svg_output_ << " Z";
        }

        svg_output_ << "\" " << fillAttr << current_style_.getEdgeStyle() << "/>\n";
    }

    void SVGConverter::processHyperbolicArc(HyperbolicArc *cmd)
    {
        if (!cmd)
            return;

        const CGMPoint &center = cmd->center();
        const CGMPoint &transverse = cmd->transverseRadius();
        const CGMPoint &conjugate = cmd->conjugateRadius();
        const CGMPoint &startPoint = cmd->startPoint();
        const CGMPoint &endPoint = cmd->endPoint();
        std::vector<CGMPoint> fallbackLine{startPoint, endPoint};

        double a = std::hypot(transverse.x(), transverse.y());
        double b = std::hypot(conjugate.x(), conjugate.y());

        if (a <= 1e-6 || b <= 1e-6)
        {
            emitSampledPolyline(fallbackLine);
            return;
        }

        auto normalize = [](const CGMPoint &v) -> CGMPoint
        {
            double len = std::hypot(v.x(), v.y());
            if (len <= 1e-12)
            {
                return CGMPoint(0.0, 0.0);
            }
            return CGMPoint(v.x() / len, v.y() / len);
        };

        auto dot = [](const CGMPoint &lhs, const CGMPoint &rhs) -> double
        {
            return lhs.x() * rhs.x() + lhs.y() * rhs.y();
        };

        CGMPoint tHat = normalize(transverse);
        CGMPoint uHat = normalize(conjugate);

        if (std::hypot(tHat.x(), tHat.y()) <= 1e-12 || std::hypot(uHat.x(), uHat.y()) <= 1e-12)
        {
            emitSampledPolyline(fallbackLine);
            return;
        }

        auto computeTheta = [&](const CGMPoint &pt, double &theta, double &signVal) -> bool
        {
            CGMPoint vec(pt.x() - center.x(), pt.y() - center.y());
            double projT = dot(vec, tHat);
            double projU = dot(vec, uHat);
            signVal = (projT >= 0.0) ? 1.0 : -1.0;
            double ratio = projU / b;
            theta = std::asinh(ratio);
            return std::isfinite(theta);
        };

        double thetaStart = 0.0;
        double thetaEnd = 0.0;
        double signStart = 1.0;
        double signEnd = 1.0;

        if (!computeTheta(startPoint, thetaStart, signStart) ||
            !computeTheta(endPoint, thetaEnd, signEnd))
        {
            emitSampledPolyline(fallbackLine);
            return;
        }

        if (signStart != signEnd)
        {
            emitSampledPolyline(fallbackLine);
            return;
        }

        auto evaluator = [&](double theta) -> CGMPoint
        {
            double coshTheta = std::cosh(theta);
            double sinhTheta = std::sinh(theta);
            return CGMPoint(
                center.x() + signStart * coshTheta * transverse.x() + sinhTheta * conjugate.x(),
                center.y() + signStart * coshTheta * transverse.y() + sinhTheta * conjugate.y());
        };

        CGMPoint startSample = evaluator(thetaStart);
        CGMPoint endSample = evaluator(thetaEnd);
        std::vector<CGMPoint> sampled;
        sampled.reserve(32);
        sampled.push_back(startSample);

        double svgUnit = transform_.transformLength(1.0);
        double toleranceSvg = 0.5;
        double toleranceVdc = (svgUnit > 1e-6) ? toleranceSvg / svgUnit : std::abs(thetaEnd - thetaStart) / 64.0;
        double toleranceSq = std::max(toleranceVdc * toleranceVdc, 1e-12);

        adaptiveSampleCurve(evaluator, thetaStart, thetaEnd, startSample, endSample, toleranceSq, 0, sampled);
        emitSampledPolyline(sampled);
    }

    void SVGConverter::processParabolicArc(ParabolicArc *cmd)
    {
        if (!cmd)
            return;

        const CGMPoint &p0 = cmd->startPoint();
        const CGMPoint &p1 = cmd->tangentIntersection();
        const CGMPoint &p2 = cmd->endPoint();

        auto evaluator = [&](double t) -> CGMPoint
        {
            double oneMinus = 1.0 - t;
            double x = oneMinus * oneMinus * p0.x() +
                       2.0 * oneMinus * t * p1.x() +
                       t * t * p2.x();
            double y = oneMinus * oneMinus * p0.y() +
                       2.0 * oneMinus * t * p1.y() +
                       t * t * p2.y();
            return CGMPoint(x, y);
        };

        CGMPoint startPoint = evaluator(0.0);
        CGMPoint endPoint = evaluator(1.0);
        std::vector<CGMPoint> sampled;
        sampled.reserve(16);
        sampled.push_back(startPoint);

        double svgUnit = transform_.transformLength(1.0);
        double toleranceSvg = 0.5;
        double toleranceVdc = (svgUnit > 1e-6) ? toleranceSvg / svgUnit : 0.02;
        double toleranceSq = std::max(toleranceVdc * toleranceVdc, 1e-12);

        adaptiveSampleCurve(evaluator, 0.0, 1.0, startPoint, endPoint, toleranceSq, 0, sampled);
        emitSampledPolyline(sampled);
    }

    void SVGConverter::processRectangle(Rectangle *cmd)
    {
        if (!cmd)
            return;

        CGMPoint p1_svg = transform_.transformPoint(cmd->firstCorner());
        CGMPoint p2_svg = transform_.transformPoint(cmd->secondCorner());

        double x = std::min(p1_svg.x(), p2_svg.x());
        double y = std::min(p1_svg.y(), p2_svg.y());
        double width = std::abs(p2_svg.x() - p1_svg.x());
        double height = std::abs(p2_svg.y() - p1_svg.y());

        if (in_figure_)
        {
            // Inside FIGURE: emit as a closed M/L/L/L/Z subpath so it
            // joins the consolidated figure path. Without this, RECT in
            // a figure block emits a standalone <rect> that overlaps
            // the figure's consolidated fill (visible in FIGURE07).
            std::ostringstream frag;
            frag.setf(std::ios::fixed);
            frag << "M " << x << " " << y
                 << " L " << (x + width) << " " << y
                 << " L " << (x + width) << " " << (y + height)
                 << " L " << x << " " << (y + height) << " Z ";
            CGMPoint sp(x, y);
            CGMPoint ep(x, y);
            figure_path_fragments_.push_back(frag.str());
            figure_ordered_subpaths_.push_back({frag.str(), sp, ep, pending_connect_to_prev_});
            pending_connect_to_prev_ = false;
            return;
        }

        std::string fillAttr = getFillAttributeForCurrentStyle();
        closeOpenDefs();
        svg_output_ << "  <rect x=\"" << x
                    << "\" y=\"" << y
                    << "\" width=\"" << width
                    << "\" height=\"" << height << "\" "
                    << clipPathAttribute()
                    << fillAttr << current_style_.getEdgeStyle() << "/>\n";
    }

    void SVGConverter::processClipRectangle(ClipRectangle *cmd)
    {
        if (!cmd)
        {
            return;
        }

        clip_rect_first_ = cmd->firstCorner();
        clip_rect_second_ = cmd->secondCorner();
        clip_rectangle_defined_ = true;

        if (clip_enabled_)
        {
            updateClipPathDefinition();
        }
    }

    void SVGConverter::processClipIndicator(ClipIndicator *cmd)
    {
        if (!cmd)
        {
            return;
        }

        clip_enabled_ = (cmd->indicator() != 0);
        if (clip_enabled_ && clip_rectangle_defined_)
        {
            updateClipPathDefinition();
        }
        else
        {
            clip_path_attribute_.clear();
        }
    }

    void SVGConverter::processProtectionRegionIndicator(ProtectionRegionIndicator *cmd)
    {
        if (!cmd)
        {
            return;
        }

        int rid = cmd->regionIndex();
        int ind = cmd->indicator();
        if (ind != 2) ind = 1;  // WebCGM restricts to CLIP (2); else OFF.

        protection_region_indicator_ = ind;
        active_protection_region_index_ = rid;

        if (ind == 2)
        {
            // Enable region clipping. Use cached clipPath if already emitted
            // for this rid; otherwise emit one from the stored region geometry.
            auto cachedIt = protection_region_clip_ids_.find(rid);
            if (cachedIt != protection_region_clip_ids_.end())
            {
                clip_path_attribute_ = " clip-path=\"url(#" + cachedIt->second + ")\" ";
            }
            else
            {
                auto defIt = protection_region_definitions_.find(rid);
                if (defIt != protection_region_definitions_.end() && !defIt->second.empty())
                {
                    if (!defs_open_)
                    {
                        svg_output_ << "  <defs>\n";
                        defs_open_ = true;
                    }
                    std::string clipId = "clipPath" + std::to_string(++clip_path_counter_);
                    svg_output_ << "    <clipPath id=\"" << clipId << "\" clipPathUnits=\"userSpaceOnUse\">\n";
                    for (const auto &p : defIt->second)
                    {
                        svg_output_ << "      <path d=\"" << p << "\" />\n";
                    }
                    svg_output_ << "    </clipPath>\n";
                    protection_region_clip_ids_[rid] = clipId;
                    clip_path_attribute_ = " clip-path=\"url(#" + clipId + ")\" ";
                }
                else if (debug_fill_logging_)
                {
                    std::cerr << "[svg] PRI ind=2 rid=" << rid
                              << " but no region geometry captured; clip not applied\n";
                }
            }
        }
        else
        {
            // Disable region clipping. Revert to whatever CLIP RECTANGLE clip
            // was active (or empty if none).
            updateClipPathDefinition();
        }

        if (debug_fill_logging_)
        {
            std::cerr << "[svg] protection region indicator region=" << rid
                      << " mode=" << ind << "\n";
        }
    }

    void SVGConverter::processText(Text *cmd)
    {
        if (!cmd)
            return;

        if (pending_text_active_)
        {
            flushPendingText();
        }

        pending_text_active_ = true;
        pending_text_position_ = cmd->position();
        pending_is_restricted_text_ = false;
        pending_text_segments_.clear();

        // Capture alignment at start of text sequence
        pending_text_h_align_ = current_style_.textHAlign();
        pending_text_v_align_ = current_style_.textVAlign();
        pending_text_rotation_deg_ = currentTextRotationDegrees();
        pending_text_has_rotation_ = std::fabs(pending_text_rotation_deg_) > 1e-6;

        // Create first segment with CURRENT attributes
        PendingTextSegment seg;
        seg.text = cmd->text();
        seg.color = current_style_.textColor();
        seg.height = current_style_.textHeight();
        seg.font_family = current_style_.fontFamily();
        seg.letter_spacing = current_style_.characterSpacing() * current_style_.textHeight();
        seg.expansion = current_style_.characterExpansion();
        pending_text_segments_.push_back(seg);

        has_last_text_position_ = true;
        last_text_position_ = cmd->position();

        if (cmd->isFinal())
        {
            flushPendingText();
        }
    }

    void SVGConverter::emitTextRun(const TextEmitterParams &input)
    {
        if (input.text_content.find_first_of("\r\n") != std::string::npos)
        {
        std::vector<std::string> lines = splitTextIntoLines(input.text_content);
        double lineAdvance = (input.line_height > 0.0) ? input.line_height : input.font_size_svg;
        std::shared_ptr<LoadedFont> metricsFont;
        if (!lines.empty())
        {
            std::string primary = primaryFontFromStack(input.font_family);
            metricsFont = acquireFontForFamily(primary);
            if (metricsFont && metricsFont->valid && input.font_size_svg > 0.0)
            {
                int ascent = 0;
                int descent = 0;
                int lineGap = 0;
                stbtt_GetFontVMetrics(metricsFont->info.get(), &ascent, &descent, &lineGap);
                double scale = stbtt_ScaleForPixelHeight(metricsFont->info.get(), static_cast<float>(input.font_size_svg));
                double metricsAdvance = (static_cast<double>(ascent - descent + lineGap)) * scale;
                if (std::isfinite(metricsAdvance) && metricsAdvance > 0.0)
                {
                    lineAdvance = metricsAdvance;
                }
            }
        }
        double radians = input.has_rotation ? input.rotation_deg * M_PI / 180.0 : 0.0;
        double cosA = std::cos(radians);
        double sinA = std::sin(radians);

            for (size_t i = 0; i < lines.size(); ++i)
            {
                TextEmitterParams lineParams = input;
                lineParams.text_content = lines[i];
                lineParams.line_height = lineAdvance;

                double deltaX = 0.0;
                double deltaY = -static_cast<double>(i) * lineAdvance;
                double worldX = input.position_svg.x() + cosA * deltaX - sinA * deltaY;
                double worldY = input.position_svg.y() + sinA * deltaX + cosA * (-deltaY);
                lineParams.position_svg = CGMPoint(worldX, worldY);

                emitTextRunSingleLine(lineParams);
            }
            return;
        }

        emitTextRunSingleLine(input);
    }

    void SVGConverter::emitTextRunSingleLine(const TextEmitterParams &input)
    {
        TextEmitterParams params = input;
        const double baselineAdjustEm = baselineAdjustmentForFont(params.font_family);
        if (std::fabs(baselineAdjustEm) > 1e-9 && params.font_size_svg > 0.0)
        {
            double radians = params.has_rotation ? params.rotation_deg * M_PI / 180.0 : 0.0;
            double offset = baselineAdjustEm * params.font_size_svg;
            double sinA = std::sin(radians);
            double cosA = std::cos(radians);
            params.position_svg = CGMPoint(params.position_svg.x() - sinA * offset,
                                           params.position_svg.y() + cosA * offset);
        }

        if (shouldEmitTextAsPath(params))
        {
            emitTextAsPath(params);
        }
        else
        {
            emitStandardText(params);
        }
    }

    bool SVGConverter::shouldEmitTextAsPath(const TextEmitterParams &params) const
    {
        if (text_options_.text_as_path)
        {
            return true;
        }
        if (text_options_.text_as_path_threshold > 0.0)
        {
            // Treat threshold as "small text" cutoff (in SVG units)
            if (params.font_size_svg <= text_options_.text_as_path_threshold)
            {
                return true;
            }
        }
        return false;
    }

    void SVGConverter::emitStandardText(const TextEmitterParams &params)
    {
        std::string escapedText = escapeXmlText(params.text_content);

        svg_output_ << "  <text x=\"" << params.position_svg.x()
                    << "\" y=\"" << params.position_svg.y() << "\" "
                    << "fill=\"" << params.color_hex << "\" ";

        if (!params.text_anchor_attr.empty())
        {
            svg_output_ << params.text_anchor_attr << " ";
        }
        if (!params.dominant_baseline_attr.empty())
        {
            svg_output_ << params.dominant_baseline_attr << " ";
        }
        if (params.apply_clip)
        {
            svg_output_ << clipPathAttribute();
        }
        if (params.has_rotation)
        {
            svg_output_ << "transform=\"rotate(" << params.rotation_deg << " "
                        << params.position_svg.x() << " " << params.position_svg.y() << ")\" ";
        }

        std::ostringstream styleBuffer;
        if (std::fabs(params.expansion - 1.0) > 1e-6)
        {
            styleBuffer << "font-stretch:" << params.expansion * 100.0 << "%;";
        }
        std::string styleString = styleBuffer.str();
        if (!styleString.empty())
        {
            svg_output_ << "style=\"" << styleString << "\" ";
        }
        if (std::fabs(params.letter_spacing) > 1e-9)
        {
            svg_output_ << "letter-spacing=\"" << params.letter_spacing << "\" ";
        }

        svg_output_ << "font-family=\"" << escapeXmlAttribute(params.font_family) << "\" ";

        // Detect bold weight and oblique/italic styles from PostScript-style
        // composite font names (e.g., "Helvetica-Bold", "Times-Italic"). The
        // family name itself rarely resolves directly in modern fontdb stacks,
        // but emitting font-weight / font-style alongside lets the resolver
        // pick the same family with the right styling. Examines the FIRST
        // family in the stack only — fallbacks shouldn't dictate styling.
        {
            std::string firstFamily = params.font_family;
            size_t comma = firstFamily.find(',');
            if (comma != std::string::npos) firstFamily.resize(comma);
            std::string fontLower = firstFamily;
            std::transform(fontLower.begin(), fontLower.end(), fontLower.begin(),
                          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (fontLower.find("bold") != std::string::npos) {
                svg_output_ << "font-weight=\"bold\" ";
            }
            if (fontLower.find("oblique") != std::string::npos) {
                svg_output_ << "font-style=\"oblique\" ";
            } else if (fontLower.find("italic") != std::string::npos) {
                svg_output_ << "font-style=\"italic\" ";
            }
        }

        svg_output_ << "font-size=\"" << params.font_size_svg << "\"";
        if (params.preserve_whitespace)
        {
            svg_output_ << " xml:space=\"preserve\"";
        }
        svg_output_ << ">";

        if (!escapedText.empty())
        {
            if (params.wrap_aps_substring)
            {
                svg_output_ << "<tspan data-aps-substring=\"true\">" << escapedText << "</tspan>";
            }
            else
            {
                svg_output_ << escapedText;
            }
        }

        svg_output_ << "</text>\n";
    }

    void SVGConverter::emitTextAsPath(const TextEmitterParams &params)
    {
        if (params.text_content.empty() || params.text_content.find('\n') != std::string::npos || params.text_content.find('\r') != std::string::npos)
        {
            // Multi-line / empty strings fall back to text for now
            emitStandardText(params);
            return;
        }

        std::string primaryFont = primaryFontFromStack(params.font_family);
        std::shared_ptr<LoadedFont> font = acquireFontForFamily(primaryFont);
        if (!font)
        {
            if (!text_as_path_warning_emitted_)
            {
                std::cerr << "[svg] text-as-path fallback to <text>; no outline font available for \"" << primaryFont << "\"\n";
                text_as_path_warning_emitted_ = true;
            }
            emitStandardText(params);
            return;
        }

        std::vector<uint32_t> codepoints = utf8ToCodepoints(params.text_content);
        if (codepoints.empty())
        {
            emitStandardText(params);
            return;
        }

        std::vector<int> glyphIndices;
        glyphIndices.reserve(codepoints.size());
        bool missingGlyph = false;
        for (uint32_t cp : codepoints)
        {
            int glyph = stbtt_FindGlyphIndex(font->info.get(), static_cast<int>(cp));
            if (glyph == 0 && cp != 0x20 && cp != 0x09)
            {
                missingGlyph = true;
            }
            glyphIndices.push_back(glyph);
        }

        if (missingGlyph)
        {
            if (!text_as_path_warning_emitted_)
            {
                std::cerr << "[svg] text-as-path fallback to <text>; missing glyph outlines in font \"" << primaryFont << "\"\n";
                text_as_path_warning_emitted_ = true;
            }
            emitStandardText(params);
            return;
        }

        int ascent = 0;
        int descent = 0;
        int lineGap = 0;
        stbtt_GetFontVMetrics(font->info.get(), &ascent, &descent, &lineGap);
        double scale = stbtt_ScaleForPixelHeight(font->info.get(), static_cast<float>(params.font_size_svg));
        double expansion = params.expansion;

        double baselineShift = 0.0;
        const std::string &baselineAttr = params.dominant_baseline_attr;
        if (baselineAttr.find("text-before-edge") != std::string::npos)
        {
            baselineShift = -static_cast<double>(ascent) * scale;
        }
        else if (baselineAttr.find("text-after-edge") != std::string::npos)
        {
            baselineShift = -static_cast<double>(descent) * scale;
        }
        else if (baselineAttr.find("central") != std::string::npos)
        {
            baselineShift = -0.5 * (static_cast<double>(ascent) + static_cast<double>(descent)) * scale;
        }
        else
        {
            baselineShift = 0.0;
        }

        struct PathCommand
        {
            char type;
            double coords[6];
            int coordCount;
        };

        std::vector<PathCommand> commands;
        commands.reserve(codepoints.size() * 8);

        double pen = 0.0;
        bool anyContours = false;

        for (size_t i = 0; i < glyphIndices.size(); ++i)
        {
            int glyphIndex = glyphIndices[i];
            auto cacheIt = font->outline_cache.find(glyphIndex);
            if (cacheIt == font->outline_cache.end())
            {
                GlyphOutline outline;
                stbtt_GetGlyphHMetrics(font->info.get(), glyphIndex, &outline.advanceWidth, &outline.leftBearing);
                stbtt_vertex *vertices = nullptr;
                int numVerts = stbtt_GetGlyphShape(font->info.get(), glyphIndex, &vertices);
                if (numVerts > 0 && vertices)
                {
                    outline.hasContours = true;
                    outline.vertices.reserve(static_cast<size_t>(numVerts) + 1);
                    for (int v = 0; v < numVerts; ++v)
                    {
                        GlyphOutlineVertex gv{};
                        gv.type = static_cast<uint8_t>(vertices[v].type);
                        gv.x = vertices[v].x;
                        gv.y = vertices[v].y;
                        gv.cx = vertices[v].cx;
                        gv.cy = vertices[v].cy;
                        gv.cx1 = vertices[v].cx1;
                        gv.cy1 = vertices[v].cy1;
                        outline.vertices.push_back(gv);
                    }
                    GlyphOutlineVertex closeV{};
                    closeV.type = static_cast<uint8_t>('Z');
                    outline.vertices.push_back(closeV);
                }
                stbtt_FreeShape(font->info.get(), vertices);
                cacheIt = font->outline_cache.emplace(glyphIndex, std::move(outline)).first;
            }

            const GlyphOutline &outline = cacheIt->second;
            double glyphOriginX = pen + static_cast<double>(outline.leftBearing) * scale * expansion;

            if (outline.hasContours)
            {
                anyContours = true;
                for (const GlyphOutlineVertex &vertex : outline.vertices)
                {
                    PathCommand cmd{};
                    if (vertex.type == STBTT_vmove)
                    {
                        cmd.type = 'M';
                        cmd.coordCount = 2;
                        cmd.coords[0] = glyphOriginX + static_cast<double>(vertex.x) * scale * expansion;
                        cmd.coords[1] = static_cast<double>(vertex.y) * scale + baselineShift;
                    }
                    else if (vertex.type == STBTT_vline)
                    {
                        cmd.type = 'L';
                        cmd.coordCount = 2;
                        cmd.coords[0] = glyphOriginX + static_cast<double>(vertex.x) * scale * expansion;
                        cmd.coords[1] = static_cast<double>(vertex.y) * scale + baselineShift;
                    }
                    else if (vertex.type == STBTT_vcurve)
                    {
                        cmd.type = 'Q';
                        cmd.coordCount = 4;
                        cmd.coords[0] = glyphOriginX + static_cast<double>(vertex.cx) * scale * expansion;
                        cmd.coords[1] = static_cast<double>(vertex.cy) * scale + baselineShift;
                        cmd.coords[2] = glyphOriginX + static_cast<double>(vertex.x) * scale * expansion;
                        cmd.coords[3] = static_cast<double>(vertex.y) * scale + baselineShift;
                    }
                    else if (vertex.type == STBTT_vcubic)
                    {
                        cmd.type = 'C';
                        cmd.coordCount = 6;
                        cmd.coords[0] = glyphOriginX + static_cast<double>(vertex.cx) * scale * expansion;
                        cmd.coords[1] = static_cast<double>(vertex.cy) * scale + baselineShift;
                        cmd.coords[2] = glyphOriginX + static_cast<double>(vertex.cx1) * scale * expansion;
                        cmd.coords[3] = static_cast<double>(vertex.cy1) * scale + baselineShift;
                        cmd.coords[4] = glyphOriginX + static_cast<double>(vertex.x) * scale * expansion;
                        cmd.coords[5] = static_cast<double>(vertex.y) * scale + baselineShift;
                    }
                    else if (vertex.type == 'Z')
                    {
                        cmd.type = 'Z';
                        cmd.coordCount = 0;
                    }
                    else
                    {
                        continue;
                    }
                    commands.push_back(cmd);
                }
            }

            double advance = static_cast<double>(outline.advanceWidth) * scale * expansion;
            if (i + 1 < glyphIndices.size())
            {
                int kern = stbtt_GetGlyphKernAdvance(font->info.get(), glyphIndex, glyphIndices[i + 1]);
                advance += static_cast<double>(kern) * scale * expansion;
            }
            pen += advance;
            if (i + 1 < glyphIndices.size())
            {
                pen += params.letter_spacing;
            }
        }

        if (!anyContours)
        {
            emitStandardText(params);
            return;
        }

        double anchorShift = 0.0;
        if (params.text_anchor_attr.find("middle") != std::string::npos)
        {
            anchorShift = -pen * 0.5;
        }
        else if (params.text_anchor_attr.find("end") != std::string::npos)
        {
            anchorShift = -pen;
        }

        double angleRadians = params.rotation_deg * M_PI / 180.0;
        double cosA = std::cos(angleRadians);
        double sinA = std::sin(angleRadians);

        auto transformPoint = [&](double localX, double localY) {
            double shiftedX = localX + anchorShift;
            double shiftedY = localY;
            double yDown = -shiftedY;
            double worldX = params.position_svg.x() + cosA * shiftedX - sinA * yDown;
            double worldY = params.position_svg.y() + sinA * shiftedX + cosA * yDown;
            return std::pair<double, double>(worldX, worldY);
        };

        std::ostringstream pathData;
        pathData.setf(std::ios::fixed);
        pathData << std::setprecision(6);

        for (const PathCommand &cmd : commands)
        {
            switch (cmd.type)
            {
            case 'M':
            case 'L':
            {
                auto [x, y] = transformPoint(cmd.coords[0], cmd.coords[1]);
                pathData << cmd.type << x << ' ' << y << ' ';
                break;
            }
            case 'Q':
            {
                auto [cx, cy] = transformPoint(cmd.coords[0], cmd.coords[1]);
                auto [x, y] = transformPoint(cmd.coords[2], cmd.coords[3]);
                pathData << 'Q' << cx << ' ' << cy << ' ' << x << ' ' << y << ' ';
                break;
            }
            case 'C':
            {
                auto [c1x, c1y] = transformPoint(cmd.coords[0], cmd.coords[1]);
                auto [c2x, c2y] = transformPoint(cmd.coords[2], cmd.coords[3]);
                auto [x, y] = transformPoint(cmd.coords[4], cmd.coords[5]);
                pathData << 'C' << c1x << ' ' << c1y << ' ' << c2x << ' ' << c2y << ' ' << x << ' ' << y << ' ';
                break;
            }
            case 'Z':
                pathData << 'Z' << ' ';
                break;
            default:
                break;
            }
        }

        std::string dString = pathData.str();
        if (!dString.empty() && std::isspace(static_cast<unsigned char>(dString.back())))
        {
            dString.pop_back();
        }
        if (dString.empty())
        {
            emitStandardText(params);
            return;
        }

        svg_output_ << "  <path d=\"" << dString << "\" fill=\"" << params.color_hex << "\"";
        if (params.apply_clip)
        {
            svg_output_ << clipPathAttribute();
        }
        if (params.wrap_aps_substring)
        {
            svg_output_ << " data-aps-substring=\"true\"";
        }
        svg_output_ << " fill-rule=\"nonzero\" />\n";
    }

    void SVGConverter::processRestrictedText(RestrictedText *cmd)
    {
        if (!cmd)
            return;

        if (pending_text_active_)
        {
            flushPendingText();
        }

        if (cmd->isFinal())
        {
            // Simple case: complete RESTRICTED TEXT in one element - render directly
            CGMPoint position_svg = transform_.transformPoint(cmd->position());
            // Use converted font size - will be scaled down if needed
            double font_size_svg = transform_.transformLength(current_style_.textHeight());

            TextEmitterParams params;
            params.position_svg = position_svg;
            params.font_size_svg = font_size_svg;
            params.line_height = font_size_svg;
            params.color_hex = colorToHexString(current_style_.textColor());
            params.rotation_deg = currentTextRotationDegrees();
            params.has_rotation = std::fabs(params.rotation_deg) > 1e-6;
            params.letter_spacing = current_style_.characterSpacing() * current_style_.textHeight();
            params.expansion = current_style_.characterExpansion();
            params.font_family = current_style_.fontFamily();
            params.text_content = cmd->text();
            params.wrap_aps_substring = false;
            params.apply_clip = true;
            // Only emit xml:space="preserve" when the text actually contains
            // whitespace characters. For single-token / numeric labels like "005"
            // or "001", the attribute is meaningless byte waste.
            params.preserve_whitespace = cmd->text().find_first_of(" \t\n\r") != std::string::npos;

            switch (current_style_.textHAlign())
            {
            case 2:
                params.text_anchor_attr = "text-anchor=\"middle\"";
                break;
            case 3:
                params.text_anchor_attr = "text-anchor=\"end\"";
                break;
            case 1:
            case 0:
            default:
                params.text_anchor_attr = "text-anchor=\"start\"";
                break;
            }

            switch (current_style_.textVAlign())
            {
            case 1: // TOP
            case 2: // CAP approximated as top
                params.dominant_baseline_attr = "dominant-baseline=\"text-before-edge\"";
                break;
            case 3: // HALF
                params.dominant_baseline_attr = "dominant-baseline=\"central\"";
                break;
            case 5: // BOTTOM
                params.dominant_baseline_attr = "dominant-baseline=\"text-after-edge\"";
                break;
            case 4: // BASE
            default:
                params.dominant_baseline_attr = "dominant-baseline=\"alphabetic\"";
                break;
            }

            emitTextRun(params);
        }
        else
        {
            // Partial text - start buffering for APPEND TEXT sequence
            pending_text_active_ = true;
            pending_text_position_ = cmd->position();
            pending_is_restricted_text_ = true;
            pending_restricted_delta_width_ = cmd->deltaWidth();
            pending_restricted_delta_height_ = cmd->deltaHeight();
            pending_text_segments_.clear();

            pending_text_h_align_ = current_style_.textHAlign();
            pending_text_v_align_ = current_style_.textVAlign();
            pending_text_rotation_deg_ = currentTextRotationDegrees();
            pending_text_has_rotation_ = std::fabs(pending_text_rotation_deg_) > 1e-6;

            PendingTextSegment seg;
            seg.text = cmd->text();
            seg.color = current_style_.textColor();
            // Use converted font size - will be scaled down in flushPendingText if needed
            seg.height = current_style_.textHeight();
            seg.font_family = current_style_.fontFamily();
            seg.letter_spacing = current_style_.characterSpacing() * current_style_.textHeight();
            seg.expansion = current_style_.characterExpansion();
            pending_text_segments_.push_back(seg);

            has_last_text_position_ = true;
            last_text_position_ = cmd->position();
        }
    }

    void SVGConverter::processAppendText(AppendText *cmd)
    {
        if (!cmd)
            return;

        if (!pending_text_active_)
        {
            // Orphan APPEND TEXT - start new sequence from last position
            pending_text_active_ = true;
            pending_text_position_ = has_last_text_position_ ? last_text_position_ : CGMPoint(0.0, 0.0);
            pending_is_restricted_text_ = false;
            pending_text_segments_.clear();
            pending_text_h_align_ = current_style_.textHAlign();
            pending_text_v_align_ = current_style_.textVAlign();
            pending_text_rotation_deg_ = currentTextRotationDegrees();
            pending_text_has_rotation_ = std::fabs(pending_text_rotation_deg_) > 1e-6;
        }

        // Create new segment with CURRENT attributes (may have changed since TEXT!)
        PendingTextSegment seg;
        seg.text = cmd->text();
        seg.color = current_style_.textColor();        // Current color (may be different)
        // Use converted height - will be scaled in flushPendingText if RESTRICTED TEXT
        seg.height = current_style_.textHeight();
        seg.font_family = current_style_.fontFamily(); // Current font (may be different)
        seg.letter_spacing = current_style_.characterSpacing() * current_style_.textHeight();
        seg.expansion = current_style_.characterExpansion();
        pending_text_segments_.push_back(seg);

        if (cmd->isFinal())
        {
            flushPendingText();
        }
    }

    void SVGConverter::flushPendingText()
    {
        if (!pending_text_active_ || pending_text_segments_.empty())
        {
            pending_text_active_ = false;
            pending_text_segments_.clear();
            return;
        }

        CGMPoint base_position_svg = transform_.transformPoint(pending_text_position_);

        // Calculate cumulative X offsets for each segment and total width
        std::vector<std::pair<double, size_t>> positioned_segments;
        double current_x_offset = 0.0;

        for (size_t i = 0; i < pending_text_segments_.size(); ++i)
        {
            const auto& seg = pending_text_segments_[i];
            positioned_segments.push_back({current_x_offset, i});

            double seg_font_size = transform_.transformLength(seg.height);
            double seg_width = measureTextWidth(seg.text, seg.font_family, seg_font_size);
            seg_width *= seg.expansion;

            if (!seg.text.empty() && seg.letter_spacing != 0.0)
            {
                seg_width += (seg.text.length() - 1) * seg.letter_spacing;
            }

            current_x_offset += seg_width;
        }

        double total_width = current_x_offset;

        // Auto-scaling for RESTRICTED TEXT per ISO 8632-1:1999 Section 7.6.5:
        // "If necessary, the values of the text attributes CHARACTER HEIGHT...
        // which are used to display this string are varied to achieve the required restriction."
        double scale_factor = 1.0;
        if (pending_is_restricted_text_)
        {
            double width_scale = 1.0;
            double height_scale = 1.0;

            constexpr double CAP_HEIGHT_RATIO = 0.7;

            // Width scaling: compare measured text width directly to extent width
            // (extent width is in VDC units, text width is measured at current font-size)
            if (pending_restricted_delta_width_ > 0.0)
            {
                double extent_width_svg = transform_.transformLength(pending_restricted_delta_width_);
                if (total_width > extent_width_svg && extent_width_svg > 0.0)
                {
                    width_scale = extent_width_svg / total_width;
                }
            }

            // Height scaling: compare CAP HEIGHTS (not font-size to extent height)
            // The extent height is specified in cap-height units (same as CHARACTER HEIGHT),
            // so we must compare cap height to cap height, not font-size (em-square) to cap height.
            if (pending_restricted_delta_height_ > 0.0)
            {
                double extent_height_svg = transform_.transformLength(pending_restricted_delta_height_);

                // Find max font size among all segments
                double max_font_size = 0.0;
                for (const auto& seg : pending_text_segments_)
                {
                    double seg_font_size = transform_.transformLength(seg.height);
                    max_font_size = std::max(max_font_size, seg_font_size);
                }

                // Calculate actual cap height: font-size × 0.7
                // (font-size is em-square, cap height is ~70% of em-square)
                double actual_cap_height = max_font_size * CAP_HEIGHT_RATIO;

                // Only scale if cap height exceeds extent height
                if (actual_cap_height > extent_height_svg && extent_height_svg > 0.0)
                {
                    height_scale = extent_height_svg / actual_cap_height;
                }
            }

            // Use the more restrictive scale factor
            scale_factor = std::min(width_scale, height_scale);

            // Recalculate positioned_segments with scaled font sizes if scaling is needed
            if (scale_factor < 1.0)
            {
                positioned_segments.clear();
                current_x_offset = 0.0;

                for (size_t i = 0; i < pending_text_segments_.size(); ++i)
                {
                    const auto& seg = pending_text_segments_[i];
                    positioned_segments.push_back({current_x_offset, i});

                    // Apply scale factor to font size for width calculation
                    double seg_font_size = transform_.transformLength(seg.height) * scale_factor;
                    double seg_width = measureTextWidth(seg.text, seg.font_family, seg_font_size);
                    seg_width *= seg.expansion;

                    if (!seg.text.empty() && seg.letter_spacing != 0.0)
                    {
                        seg_width += (seg.text.length() - 1) * seg.letter_spacing * scale_factor;
                    }

                    current_x_offset += seg_width;
                }

                total_width = current_x_offset;
            }
        }

        double aligned_x = base_position_svg.x();
        switch (pending_text_h_align_)
        {
        case 2: aligned_x -= total_width / 2.0; break;
        case 3: aligned_x -= total_width; break;
        default: break;
        }

        std::string dominant_baseline_attr;
        switch (pending_text_v_align_)
        {
        case 1: case 2:
            dominant_baseline_attr = "dominant-baseline=\"text-before-edge\"";
            break;
        case 3:
            dominant_baseline_attr = "dominant-baseline=\"central\"";
            break;
        case 5:
            dominant_baseline_attr = "dominant-baseline=\"text-after-edge\"";
            break;
        case 4: default:
            dominant_baseline_attr = "dominant-baseline=\"alphabetic\"";
            break;
        }

        for (const auto& [x_offset, seg_idx] : positioned_segments)
        {
            const auto& seg = pending_text_segments_[seg_idx];

            TextEmitterParams params;
            params.position_svg = CGMPoint(aligned_x + x_offset, base_position_svg.y());
            // Apply scale factor for RESTRICTED TEXT auto-scaling
            params.font_size_svg = transform_.transformLength(seg.height) * scale_factor;
            params.line_height = params.font_size_svg;
            params.color_hex = colorToHexString(seg.color);
            params.font_family = seg.font_family;
            params.letter_spacing = seg.letter_spacing * scale_factor;
            params.expansion = seg.expansion;
            params.text_content = seg.text;
            params.rotation_deg = pending_text_rotation_deg_;
            params.has_rotation = pending_text_has_rotation_;
            params.text_anchor_attr = "text-anchor=\"start\"";
            params.dominant_baseline_attr = dominant_baseline_attr;
            params.wrap_aps_substring = !aps_stack_.empty();
            params.apply_clip = false;
            // See line ~7154 — only preserve whitespace when the text actually has any.
            params.preserve_whitespace = seg.text.find_first_of(" \t\n\r") != std::string::npos;

            emitTextRun(params);
        }

        // Record the baseline end position (not the start) so a subsequent
        // orphan APPEND TEXT continues the text run horizontally instead of
        // stacking on top of the previous. total_width is in SVG-space units;
        // convert back to VDC so last_text_position_ stays in the CGM domain
        // that APPEND TEXT expects.
        has_last_text_position_ = true;
        {
            double vdc_x1, vdc_y1, vdc_x2, vdc_y2;
            transform_.getVdcExtent(vdc_x1, vdc_y1, vdc_x2, vdc_y2);
            double vdc_width = std::abs(vdc_x2 - vdc_x1);
            double svg_width = transform_.transformLength(vdc_width > 0.0 ? vdc_width : 1.0);
            double vdc_advance = (svg_width > 0.0 && vdc_width > 0.0) ? (total_width * vdc_width / svg_width) : 0.0;
            last_text_position_ = CGMPoint(pending_text_position_.x() + vdc_advance,
                                           pending_text_position_.y());
        }
        pending_text_active_ = false;
        pending_text_segments_.clear();
        pending_is_restricted_text_ = false;
        pending_restricted_delta_width_ = 0.0;
        pending_restricted_delta_height_ = 0.0;
        pending_text_has_rotation_ = false;
        pending_text_rotation_deg_ = 0.0;
    }

    void SVGConverter::processLineColor(LineColour *cmd)
    {
        if (!cmd)
            return;

        Color resolved = resolveColor(cmd->color(), ColorRole::Stroke, "LINE COLOUR");
        // Two-pots model: store into the slot for the mode this command was
        // parsed under, then make it active.
        if (cmd->color().isIndexed()) {
            line_color_slots_.indexed = resolved;
        } else {
            line_color_slots_.direct = resolved;
        }
        current_style_.setLineColor(resolved);
        current_style_.markLineColorExplicit();
    }

    void SVGConverter::processFillColor(FillColour *cmd)
    {
        if (!cmd)
            return;

        Color resolved = resolveColor(cmd->color(), ColorRole::Fill, "FILL COLOUR");
        if (cmd->color().isIndexed()) {
            fill_color_slots_.indexed = resolved;
        } else {
            fill_color_slots_.direct = resolved;
        }
        current_style_.setFillColor(resolved);
        current_style_.markFillColorExplicit();
        if (debug_fill_logging_)
        {
            std::cerr << "[svg] set fill color -> " << static_cast<int>(resolved.r) << ","
                      << static_cast<int>(resolved.g) << ","
                      << static_cast<int>(resolved.b);
            if (cmd->color().isIndexed())
            {
                std::cerr << " (index=" << cmd->color().colorIndex() << ")";
            }
            std::cerr << "\n";
        }
    }

    void SVGConverter::processLineWidth(LineWidth *cmd)
    {
        if (!cmd)
            return;

        svg::StrokeWidthContext ctx;
        ctx.spec_mode = line_width_spec_mode_;
        ctx.nominal_width_svg = nominal_line_width_svg_;
        ctx.abstract_width_unit = abstract_line_width_unit_;
        ctx.picture_longest_side_raw = picture_longest_side_raw_;
        ctx.picture_vdc_width = picture_vdc_width_;
        ctx.picture_vdc_height = picture_vdc_height_;
        ctx.viewbox_x1 = svg_bounds_x1_;
        ctx.viewbox_y1 = svg_bounds_y1_;
        ctx.viewbox_x2 = svg_bounds_x2_;
        ctx.viewbox_y2 = svg_bounds_y2_;
        ctx.transform = &transform_;

        double svg_width = svg::computeStrokeWidth(
            cmd->width(), ctx, width_logging_enabled_, "line-width");

        current_style_.setLineWidth(svg_width);
    }

    void SVGConverter::processLineType(LineType *cmd)
    {
        if (!cmd)
            return;

        int type = cmd->type();
        current_style_.setLineType(type);

        // If the metafile installed a custom dash pattern via LINE AND EDGE
        // TYPE DEFINITION for this index, attach it to the paint state so
        // getStrokeDashPattern() picks it up. Without this, vendor-extended
        // line types (typically negative — LINSTD01 uses -1..-8) fall through
        // to the "no pattern -> solid line" default and silently lose the
        // intended dashed/dotted rendering.
        auto it = line_type_definitions_.find(type);
        if (it != line_type_definitions_.end() && !it->second.empty())
        {
            std::ostringstream oss;
            oss.setf(std::ios::fixed);
            for (size_t i = 0; i < it->second.size(); ++i)
            {
                if (i > 0) oss << ",";
                oss << it->second[i];
            }
            current_style_.setLineTypeCustomDash(oss.str());
        }
    }

    void SVGConverter::processLineAndEdgeTypeDefinition(LineAndEdgeTypeDefinition *cmd)
    {
        if (!cmd)
            return;

        // ISO 8632-1 §6.5.27 LINE AND EDGE TYPE DEFINITION layout:
        //   P1 line type (I) — already consumed by the reader as cmd->lineType()
        //   P2 dash cycle repeat length (R) — total cycle length, not a dash
        //   P3 dash element list (I[N]) — alternating mark/gap lengths
        //
        // The reader collapses P2 and P3 into one int vector (cmd->dashPattern()),
        // so the first entry is the cycle length and the rest are the dash
        // elements. Treating the cycle length as a dash produced LINSTD01's
        // long solid prefix; skip it here.
        //
        // Preserve zero-length dashes — a 0-mark inside SVG stroke-dasharray
        // produces "no mark, advance the gap" semantics, which lets
        // LINSTD01's "decreasing" pattern collapse adjacent gaps cleanly.
        // Earlier code clamped len to 1.0 to avoid an assumed sub-pixel
        // render artefact, but that introduced visible tick marks.
        std::vector<double> svgDash;
        const auto &raw = cmd->dashPattern();
        svgDash.reserve(raw.size() > 0 ? raw.size() - 1 : 0);
        for (size_t i = 1; i < raw.size(); ++i)
        {
            int v = raw[i];
            double len = transform_.transformLength(static_cast<double>(std::abs(v)));
            if (len < 0.0) len = 0.0;
            svgDash.push_back(len);
        }
        line_type_definitions_[cmd->lineType()] = std::move(svgDash);

        // If this definition installs a pattern for the CURRENTLY ACTIVE line
        // type, refresh the paint state so subsequent strokes pick it up
        // immediately (CGMs may emit DEFINITION then LINE TYPE in either order).
        auto it = line_type_definitions_.find(cmd->lineType());
        std::string dashStr;
        if (it != line_type_definitions_.end())
        {
            std::ostringstream oss;
            oss.setf(std::ios::fixed);
            for (size_t i = 0; i < it->second.size(); ++i)
            {
                if (i > 0) oss << ",";
                oss << it->second[i];
            }
            dashStr = oss.str();
        }
        if (cmd->lineType() == current_style_.lineType() && !dashStr.empty())
        {
            current_style_.setLineType(cmd->lineType());
            current_style_.setLineTypeCustomDash(dashStr);
        }
        // Same definition table also applies to EDGE TYPE — refresh the
        // active edge dash if this index matches the current edge_type_.
        if (cmd->lineType() == current_style_.edgeType() && !dashStr.empty())
        {
            current_style_.setEdgeType(cmd->lineType());
            current_style_.setEdgeTypeCustomDash(dashStr);
        }
    }

    void SVGConverter::processLineTypeContinuation(LineTypeContinuation *cmd)
    {
        if (!cmd)
            return;

        bool cont = cmd->mode() != 0;
        current_style_.setLineDashContinuation(cont);
        if (!cont)
        {
            current_style_.setLineDashOffset(0.0);
        }
    }

    void SVGConverter::processLineTypeInitialOffset(LineTypeInitialOffset *cmd)
    {
        if (!cmd)
            return;

        double offset_value = cmd->offset();
        double svg_offset = transform_.transformLength(offset_value);
        current_style_.setLineDashOffset(svg_offset);
    }

    void SVGConverter::processTextFontIndex(TextFontIndex *cmd)
    {
        if (!cmd)
            return;

        int index = std::max(cmd->index(), 1);
        current_style_.setFontIndex(index);
        current_style_.setFontFamily(resolveFontFamilyFromIndex(index));
    }

    void SVGConverter::processTextPrecision(TextPrecision *cmd)
    {
        if (!cmd)
            return;

        current_style_.setTextPrecision(static_cast<int>(cmd->value()));
    }

    void SVGConverter::processCharacterExpansion(CharacterExpansionFactor *cmd)
    {
        if (!cmd)
            return;

        current_style_.setCharacterExpansion(cmd->factor());
    }

    void SVGConverter::processCharacterSpacing(CharacterSpacing *cmd)
    {
        if (!cmd)
            return;

        // ISO 8632-1 §6.5.36: CHARACTER SPACING is "additional inter-character
        // spacing in units of CHARACTER HEIGHT", a multiplier — not a VDC
        // length. Store the raw value; multiply by the active character
        // height at text emission so changes to CHARACTER HEIGHT don't
        // require recomputing the stored spacing. Previously this passed
        // through transformLength, treating the value as VDC and producing
        // ~10% character-width spacing for "Spacing 5.0" instead of 5×.
        current_style_.setCharacterSpacing(cmd->spacing());
    }

    void SVGConverter::processTextPath(Command *cmd)
    {
        (void)cmd;
        // Text path handling is managed via CHARACTER ORIENTATION for now.
    }

    void SVGConverter::processInteriorStyle(InteriorStyle *cmd)
    {
        if (!cmd)
            return;

        int style = static_cast<int>(cmd->style());
        current_style_.setFillStyle(style);
        if (debug_fill_logging_)
        {
            std::cerr << "[svg] set interior style -> " << interiorStyleName(style) << " (value=" << style << ")\n";
        }
    }

    void SVGConverter::processHatchIndex(HatchIndex *cmd)
    {
        if (!cmd)
            return;

        int index = cmd->index();
        // Store the hatch index
        current_style_.setHatchIndex(index);

        // IMPORTANT: Per CGM spec, HATCH INDEX only selects which hatch pattern to use
        // when the current INTERIOR STYLE is already HATCH(3).
        // It does NOT change the interior style itself.
        // Only INTERIOR STYLE commands (processInteriorStyle) should change fill_style_.
    }

    void SVGConverter::processPatternIndex(PatternIndex *cmd)
    {
        if (!cmd)
            return;

        current_style_.setPatternIndex(cmd->index());
        last_defined_pattern_index_ = cmd->index();
    }

    void SVGConverter::processFillBundleIndex(FillBundleIndex *cmd)
    {
        if (!cmd)
        {
            return;
        }

        int index = std::max(cmd->index(), 1);
        if (debug_fill_logging_)
        {
            std::cerr << "[svg] set fill bundle index -> " << index << "\n";
        }
        applyFillBundle(index);
    }

    void SVGConverter::processFillRepresentation(FillRepresentation *cmd)
    {
        if (!cmd)
        {
            return;
        }

        int index = std::max(cmd->bundleIndex(), 1);
        FillBundleEntry entry;
        entry.interiorStyle = cmd->interiorStyle();
        entry.color = cmd->color();
        entry.hatchIndex = std::max(cmd->hatchIndex(), 1);
        entry.patternIndex = std::max(cmd->patternIndex(), 1);
        fill_bundles_[index] = entry;

        if (debug_fill_logging_)
        {
            std::cerr << "[svg] fill-representation index=" << index
                      << " style=" << interiorStyleName(entry.interiorStyle)
                      << " color=" << entry.color.toString()
                      << " hatch=" << entry.hatchIndex
                      << " pattern=" << entry.patternIndex << "\n";
        }

        if (index == active_fill_bundle_index_)
        {
            applyFillBundleEntry(entry, "fill-representation");
        }
    }

    void SVGConverter::processPatternTable(PatternTable *cmd)
    {
        if (!cmd)
            return;

        PatternTableData data;
        data.nx = cmd->nx();
        data.ny = cmd->ny();
        data.cells.reserve(static_cast<size_t>(std::max(0, data.nx * data.ny)));
        for (const auto &cgmColor : cmd->colors())
        {
            data.cells.push_back(resolveColor(cgmColor, ColorRole::Pattern));
        }
        pattern_tables_[cmd->index()] = std::move(data);
        pattern_fill_ids_.erase(cmd->index());
        last_defined_pattern_index_ = cmd->index();

        // If a global PATTERN SIZE was already set when this table is defined,
        // auto-bind the size to this index so the table inherits it. Handles
        // CGMs that emit PATTERN SIZE first then many PATTERN TABLEs (PATTBL01).
        // Tables defined BEFORE the first PATTERN SIZE remain unbound and fall
        // through to the viewbox-derived default at draw time (INTSTL08 1-5).
        if (has_active_pattern_size_ && pattern_sizes_.find(cmd->index()) == pattern_sizes_.end())
        {
            pattern_sizes_[cmd->index()] = active_pattern_size_;
        }
    }

    void SVGConverter::processPatternSize(PatternSize *cmd)
    {
        if (!cmd)
            return;

        PatternSizeData data;
        data.widthX = cmd->widthX();
        data.widthY = cmd->widthY();
        data.heightX = cmd->heightX();
        data.heightY = cmd->heightY();

        // Bind to the most recently defined pattern table (legacy convention
        // followed by many CGM authoring tools — INTSTL* and similar rely on
        // this) AND update the global active size (per ISO 8632-1 §6.5.21,
        // PATTERN SIZE applies to subsequent fills regardless of index —
        // PATTBL01 emits one PATTERN SIZE for 64 tables and relies on this).
        pattern_sizes_[last_defined_pattern_index_] = data;
        pattern_fill_ids_.erase(last_defined_pattern_index_);

        // ISO 8632-1 §6.5.21: PATTERN SIZE is a global attribute that applies
        // to subsequent fills. INTSTL05 emits PATTERN TABLEs 1–6 first, then
        // ONE PATTERN SIZE — without retroactive binding, tables 1–5 fall
        // through to the viewbox/16 default at draw time and render at the
        // wrong scale (62.5 units instead of the spec'd 32 units).
        // Retroactively bind to ALL existing pattern tables that don't yet
        // have an explicit per-index size.
        for (const auto &kv : pattern_tables_)
        {
            int idx = kv.first;
            if (pattern_sizes_.find(idx) == pattern_sizes_.end())
            {
                pattern_sizes_[idx] = data;
                pattern_fill_ids_.erase(idx);
            }
        }

        active_pattern_size_ = data;
        has_active_pattern_size_ = true;
    }

    void SVGConverter::processHatchStyleDefinition(HatchStyleDefinition *cmd)
    {
        if (!cmd)
        {
            return;
        }

        HatchDefinition def;
        def.styleIndicator = cmd->styleIndicator();
        if (def.styleIndicator == 0)
        {
            def.direction = cmd->direction();
            def.spacing = cmd->spacing();
        }
        hatch_definitions_[cmd->hatchIndex()] = def;
        hatch_pattern_ids_.clear();
    }

    void SVGConverter::processFillReferencePoint(FillReferencePoint *cmd)
    {
        if (!cmd)
            return;

        fill_reference_point_ = cmd->point();
        has_fill_reference_point_ = true;
        pattern_fill_ids_.clear();
        hatch_pattern_ids_.clear();
    }

    void SVGConverter::processEdgeWidth(EdgeWidth *cmd)
    {
        if (!cmd)
            return;

        svg::StrokeWidthContext ctx;
        ctx.spec_mode = edge_width_spec_mode_;
        ctx.nominal_width_svg = nominal_edge_width_svg_;
        ctx.abstract_width_unit = abstract_edge_width_unit_;
        ctx.picture_longest_side_raw = picture_longest_side_raw_;
        ctx.picture_vdc_width = picture_vdc_width_;
        ctx.picture_vdc_height = picture_vdc_height_;
        ctx.viewbox_x1 = svg_bounds_x1_;
        ctx.viewbox_y1 = svg_bounds_y1_;
        ctx.viewbox_x2 = svg_bounds_x2_;
        ctx.viewbox_y2 = svg_bounds_y2_;
        ctx.transform = &transform_;

        double svg_width = svg::computeStrokeWidth(
            cmd->width(), ctx, width_logging_enabled_, "edge-width");

        current_style_.setEdgeWidth(svg_width);
    }

    void SVGConverter::processEdgeColor(EdgeColour *cmd)
    {
        if (!cmd)
            return;

        // Resolve indexed colors through the color table
        Color resolved = resolveColor(cmd->color(), ColorRole::Edge, "EDGE COLOUR");
        if (cmd->color().isIndexed()) {
            edge_color_slots_.indexed = resolved;
        } else {
            edge_color_slots_.direct = resolved;
        }
        current_style_.setEdgeColor(resolved);
        current_style_.markEdgeColorExplicit();
    }

    void SVGConverter::processEdgeType(EdgeType *cmd)
    {
        if (!cmd)
            return;

        int type = cmd->type();
        current_style_.setEdgeType(type);

        // Mirror processLineType: ISO 8632-1 says LINE AND EDGE TYPE
        // DEFINITION provides shared dash tables for both. If a user-
        // defined dash pattern was installed for this type index, attach
        // it to PaintState so getEdgeDashPattern() picks it up. Without
        // this, samples like LINSTD03 (which uses negative-index custom
        // edge types: decreasing, long gaps, etc.) silently render solid.
        auto it = line_type_definitions_.find(type);
        if (it != line_type_definitions_.end() && !it->second.empty())
        {
            std::ostringstream oss;
            oss.setf(std::ios::fixed);
            for (size_t i = 0; i < it->second.size(); ++i)
            {
                if (i > 0) oss << ",";
                oss << it->second[i];
            }
            current_style_.setEdgeTypeCustomDash(oss.str());
        }
    }

    void SVGConverter::processEdgeTypeContinuation(EdgeTypeContinuation *cmd)
    {
        if (!cmd)
            return;

        bool cont = cmd->mode() != 0;
        current_style_.setEdgeDashContinuation(cont);
        if (!cont)
        {
            current_style_.setEdgeDashOffset(0.0);
        }
    }

    void SVGConverter::processEdgeTypeInitialOffset(EdgeTypeInitialOffset *cmd)
    {
        if (!cmd)
            return;

        double offset_value = cmd->offset();
        double svg_offset = transform_.transformLength(offset_value);
        current_style_.setEdgeDashOffset(svg_offset);
    }

    void SVGConverter::processEdgeVisibility(EdgeVisibility *cmd)
    {
        if (!cmd)
            return;

        bool visible = cmd->isVisible();
        current_style_.setEdgeVisibility(visible);
    }

    void SVGConverter::processColourSelectionMode(ColourSelectionMode *cmd)
    {
        if (!cmd)
            return;

        ColorSelectionMode newMode = cmd->mode();
        if (newMode == active_color_mode_)
            return;

        // ISO 8632-1 §6.3.2: each colour attribute keeps independent values
        // for INDEXED vs DIRECT. Switching modes activates the other slot.
        // Save the current PaintState colors back to the now-leaving mode's
        // slot (in case any indirect updates happened) before swapping.
        if (active_color_mode_ == ColorSelectionMode::DIRECT) {
            line_color_slots_.direct = current_style_.lineColor();
            fill_color_slots_.direct = current_style_.fillColor();
            edge_color_slots_.direct = current_style_.edgeColor();
            text_color_slots_.direct = current_style_.textColor();
        } else {
            line_color_slots_.indexed = current_style_.lineColor();
            fill_color_slots_.indexed = current_style_.fillColor();
            edge_color_slots_.indexed = current_style_.edgeColor();
            text_color_slots_.indexed = current_style_.textColor();
        }

        if (newMode == ColorSelectionMode::DIRECT) {
            current_style_.setLineColor(line_color_slots_.direct);
            current_style_.setFillColor(fill_color_slots_.direct);
            current_style_.setEdgeColor(edge_color_slots_.direct);
            current_style_.setTextColor(text_color_slots_.direct);
        } else {
            current_style_.setLineColor(line_color_slots_.indexed);
            current_style_.setFillColor(fill_color_slots_.indexed);
            current_style_.setEdgeColor(edge_color_slots_.indexed);
            current_style_.setTextColor(text_color_slots_.indexed);
        }
        active_color_mode_ = newMode;

        if (debug_fill_logging_) {
            std::cerr << "[svg] CSM swap -> "
                      << (newMode == ColorSelectionMode::DIRECT ? "DIRECT" : "INDEXED")
                      << " fill=" << static_cast<int>(current_style_.fillColor().r) << ","
                      << static_cast<int>(current_style_.fillColor().g) << ","
                      << static_cast<int>(current_style_.fillColor().b) << "\n";
        }
    }

    void SVGConverter::processColourTable(ColourTable *cmd)
    {
        if (!cmd)
        {
            return;
        }

        int startIndex = cmd->startIndex();
        const auto &colors = cmd->colors();

        if (color_logging_enabled_)
        {
            std::cerr << "[color-table] start=" << startIndex << " count=" << colors.size() << "\n";
        }

        bool index1Updated = false;
        for (size_t i = 0; i < colors.size(); i++)
        {
            const Color &raw = colors[i];
            Color scaled = applyColourValueExtent(raw);
            int idx = startIndex + static_cast<int>(i);
            color_table_[idx] = scaled;
            if (idx == 1)
            {
                index1Updated = true;
            }

            if (color_logging_enabled_)
            {
                std::cerr << "  -> index " << idx
                          << " raw=(" << static_cast<int>(raw.r) << ","
                          << static_cast<int>(raw.g) << ","
                          << static_cast<int>(raw.b) << ")"
                          << " rgb(" << static_cast<int>(scaled.r) << ","
                          << static_cast<int>(scaled.g) << ","
                          << static_cast<int>(scaled.b) << ")\n";
            }
        }

        if (index1Updated)
        {
            refreshDefaultIndexedColors();
        }
    }

    // ISO 8632-1 §A.1: in INDEXED colour-selection mode, the default
    // LINE/FILL/EDGE/TEXT/MARKER COLOUR is colour-index-1. When the
    // CGM hasn't emitted an explicit *_COLOUR command yet, refresh
    // the corresponding current_style_ fields and slots.indexed from
    // color_table_[1]. Called both at picture body start (after PD
    // COLR-TABLE entries are processed) and on any subsequent
    // COLR-TABLE update of index 1.
    void SVGConverter::refreshDefaultIndexedColors()
    {
        if (active_color_mode_ != ColorSelectionMode::INDEXED)
        {
            return;
        }
        auto it = color_table_.find(1);
        if (it == color_table_.end())
        {
            return;
        }
        const Color &fg = it->second;
        if (!current_style_.lineColorExplicit())
        {
            line_color_slots_.indexed = fg;
            current_style_.setLineColor(fg);
        }
        if (!current_style_.fillColorExplicit())
        {
            fill_color_slots_.indexed = fg;
            current_style_.setFillColor(fg);
        }
        if (!current_style_.edgeColorExplicit())
        {
            edge_color_slots_.indexed = fg;
            current_style_.setEdgeColor(fg);
        }
        if (!current_style_.textColorExplicit())
        {
            text_color_slots_.indexed = fg;
            current_style_.setTextColor(fg);
        }
        if (!current_style_.markerColorExplicit())
        {
            current_style_.setMarkerColor(fg);
        }
    }

    void SVGConverter::initializeDefaultColorTable()
    {
        std::vector<Color> palette;
       palette.reserve(256);

        // ISO 8632 standard base palette (indices 0-7)
        const Color baseColors[] = {
            Color(255, 255, 255), // 0 - White (background)
            Color(0, 0, 0),       // 1 - Black (foreground)
            Color(255, 0, 0),     // 2 - Red
            Color(0, 255, 0),     // 3 - Green
            Color(0, 0, 255),     // 4 - Blue
            Color(255, 255, 0),   // 5 - Yellow (ISO 8632)
            Color(255, 0, 255),   // 6 - Magenta
            Color(0, 255, 255)    // 7 - Cyan (ISO 8632)
        };

        palette.insert(palette.end(), std::begin(baseColors), std::end(baseColors));

        auto appendUnique = [&palette](const Color &color) {
            if (palette.size() >= 256)
            {
                return;
            }
            if (std::find(palette.begin(), palette.end(), color) == palette.end())
            {
                palette.push_back(color);
            }
        };

        // WebCGM colour cube: 5 red × 9 green × 5 blue, in sRGB space.
        const int redLevels[5] = {0, 64, 128, 192, 255};
        const int greenLevels[9] = {0, 32, 64, 96, 128, 160, 192, 224, 255};
        const int blueLevels[5] = {0, 64, 128, 192, 255};

        for (int g : greenLevels)
        {
            for (int r : redLevels)
            {
                for (int b : blueLevels)
                {
                    appendUnique(Color(static_cast<uint8_t>(r),
                                        static_cast<uint8_t>(g),
                                        static_cast<uint8_t>(b)));
                }
            }
        }

        // Append 32 grayscale levels (0 → 255) as per Annex D.3.2 guidance.
        for (int i = 0; i < 32; ++i)
        {
            int value = static_cast<int>(std::round(static_cast<double>(i) * 255.0 / 31.0));
            appendUnique(Color(static_cast<uint8_t>(value),
                               static_cast<uint8_t>(value),
                               static_cast<uint8_t>(value)));
        }

        if (compatibility_mode_ && palette.size() > 2)
        {
            // Neutralize palette index 2 only when compatibility heuristics are enabled.
            palette[2] = Color(48, 48, 48);
        }

        // Ensure the palette has 256 entries; pad with black if necessary.
        while (palette.size() < 256)
        {
            palette.push_back(Color::Black());
        }

        for (size_t i = 0; i < palette.size() && i < 256; ++i)
        {
            color_table_[static_cast<int>(i)] = palette[i];
        }
    }

    void SVGConverter::recordAPSMetadata(const APSNode &node,
                                         const std::map<std::string, std::string> &attributes)
    {
        APSMetadataEntry entry;
        entry.identifier = node.identifier;
        entry.resolved_identifier = node.resolved_identifier;
        entry.type = node.type;
        entry.inherit = node.inheritanceFlag;
        entry.attributes = attributes;
        if (entry.attributes.find("apsid") == entry.attributes.end() && !node.identifier.empty())
        {
            entry.attributes["apsid"] = node.identifier;
        }
        aps_metadata_entries_.push_back(std::move(entry));
    }

    void SVGConverter::logColorResolution(int index,
                                          const Color &color,
                                          ColorRole role,
                                          const std::string &source,
                                          bool overrideApplied,
                                          bool fromTable,
                                          bool isIndexed)
    {
        if (!color_logging_enabled_)
        {
            return;
        }

        std::ostringstream keyBuilder;
        keyBuilder << svg::ColorResolver::roleName(role) << "|";
        if (isIndexed)
        {
            keyBuilder << index;
        }
        else
        {
            keyBuilder << "direct";
        }
        if (!source.empty())
        {
            keyBuilder << "|" << source;
        }
        std::string key = keyBuilder.str();

        auto it = last_logged_colors_.find(key);
        if (it != last_logged_colors_.end() && it->second == color)
        {
            return;
        }
        last_logged_colors_[key] = color;

        std::ostringstream message;
        message << "[color] role=" << svg::ColorResolver::roleName(role);
        if (isIndexed)
        {
            message << " index=" << index;
        }
        else
        {
            message << " direct";
        }
        if (!source.empty())
        {
            message << " source=\"" << source << "\"";
        }
        message << " rgb("
                << static_cast<int>(color.r) << ","
                << static_cast<int>(color.g) << ","
                << static_cast<int>(color.b) << ")";
        if (overrideApplied)
        {
            message << " override";
        }
        else if (fromTable)
        {
            message << " table";
        }
        std::cerr << message.str() << "\n";
    }

    Color SVGConverter::resolveColor(const CGMColor &cgmColor,
                                     ColorRole role,
                                     const char *debugLabel)
    {
        svg::ColorOverride override_config;
        switch (palette_override_.mode)
        {
        case PaletteOverride::Mode::Monochrome:
            override_config.mode =
                svg::PaletteOverrideMode::Monochrome;
            break;
        case PaletteOverride::Mode::Custom:
            override_config.mode =
                svg::PaletteOverrideMode::Custom;
            break;
        case PaletteOverride::Mode::None:
            override_config.mode =
                svg::PaletteOverrideMode::None;
            break;
        }
        override_config.custom_palette =
            &palette_override_.customPalette;
        override_config.apply_to_fills =
            palette_override_.applyToFills;

        const auto resolution = svg::ColorResolver::resolve(
            cgmColor,
            role,
            color_table_,
            override_config,
            colour_value_extent_min_,
            colour_value_extent_max_);

        const bool shouldLog = color_logging_enabled_ &&
                               role != ColorRole::Pattern &&
                               role != ColorRole::Raster;
        if (shouldLog)
        {
            std::string source;
            if (debugLabel && debugLabel[0] != '\0')
            {
                source = debugLabel;
            }
            else if (cgmColor.isIndexed())
            {
                source =
                    "index " +
                    std::to_string(resolution.index);
            }
            else
            {
                source = "direct";
            }
            logColorResolution(resolution.index,
                               resolution.color,
                               role,
                               source,
                               resolution.override_applied,
                               resolution.from_table,
                               resolution.indexed);
        }

        return resolution.color;
    }

    void SVGConverter::processBeginProtectionRegion(BeginProtectionRegion *cmd)
    {
        // Begin defining a region's shape. All graphical primitives between
        // BPR and EPR are captured into protection_region_paths_ as SVG path
        // data (suppressed from the visible output) and stashed under this rid
        // at EPR. Clipping itself is enabled later by PROTECTION REGION
        // INDICATOR (rid, ind=2).
        in_protection_region_ = true;
        protection_region_paths_.clear();
        active_protection_region_index_ = cmd ? cmd->regionIndex() : -1;
        if (debug_fill_logging_)
        {
            std::cerr << "[svg] begin protection region idx=" << active_protection_region_index_ << "\n";
        }
    }

    void SVGConverter::processEndProtectionRegion(EndProtectionRegion *cmd)
    {
        (void)cmd;

        if (in_protection_region_ && active_protection_region_index_ >= 0 &&
            !protection_region_paths_.empty())
        {
            // Stash captured geometry under this rid. Append (don't replace)
            // because the same rid can be re-opened across multiple BPR/EPR
            // pairs to incrementally build a complex region shape.
            auto &dest = protection_region_definitions_[active_protection_region_index_];
            for (auto &p : protection_region_paths_)
            {
                dest.push_back(std::move(p));
            }
            // Adding/changing region geometry invalidates any previously
            // emitted clipPath cached for this rid.
            protection_region_clip_ids_.erase(active_protection_region_index_);
        }

        if (debug_fill_logging_)
        {
            std::cerr << "[svg] end protection region idx=" << active_protection_region_index_
                      << " captured=" << protection_region_paths_.size() << "\n";
        }

        in_protection_region_ = false;
        protection_region_paths_.clear();
        // Note: do NOT reset active_protection_region_index_ — PROTECTION
        // REGION INDICATOR can fire later with the same rid and we need to
        // know which region it refers to in the meantime. The next BPR or
        // PRI will overwrite it anyway.
    }

    void SVGConverter::processTransparentCellColour(TransparentCellColour *cmd)
    {
        if (!cmd || !transparent_cell_colour_enabled_)
            return;

        transparent_cell_color_ = resolveColor(cmd->color(), ColorRole::Raster);
        transparent_cell_active_ = true;
        if (debug_fill_logging_)
        {
            std::cerr << "[svg] TransparentCellColour set to "
                      << static_cast<int>(transparent_cell_color_.r) << ","
                      << static_cast<int>(transparent_cell_color_.g) << ","
                      << static_cast<int>(transparent_cell_color_.b)
                      << " index=" << cmd->color().colorIndex() << "\n";
        }
    }

    bool SVGConverter::capturingProtectionRegionClip() const
    {
        // In CGM, geometry inside BEGIN/END PROTECTION REGION defines the
        // region's shape and is NOT visibly drawn — it gets captured into
        // protection_region_paths_ and later emitted as an SVG <clipPath>
        // when PROTECTION REGION INDICATOR enables clipping. The indicator
        // value is irrelevant during capture; it only controls whether
        // SUBSEQUENT primitives are clipped.
        return in_protection_region_;
    }

    void SVGConverter::processBeginFigure(Command *cmd)
    {
        (void)cmd; // Unused parameter
        in_figure_ = true;
        figure_connects_subpaths_ = false;
        pending_connect_to_prev_ = false;
        figure_polylines_.clear();
        figure_path_fragments_.clear();
        figure_ordered_subpaths_.clear();
    }

    void SVGConverter::processEndFigure(Command *cmd)
    {
        (void)cmd; // Unused parameter
        if (in_figure_)
        {
            renderFigure();
            in_figure_ = false;
            figure_polylines_.clear();
            figure_path_fragments_.clear();
            figure_ordered_subpaths_.clear();
        }
    }

    void SVGConverter::processConnectingEdge(ConnectingEdge *cmd)
    {
        (void)cmd;
        if (!in_figure_)
        {
            svg_output_ << "  <!-- CONNECTING EDGE ignored: outside FIGURE scope (ISO/IEC 8632-1) -->\n";
            return;
        }

        figure_connects_subpaths_ = true;
        // ISO 8632-1 §7.6.21: CONNECTING EDGE inside a FIGURE adds an
        // explicit straight edge from the current path position to the
        // start of the NEXT primitive. Capture as a transient flag the
        // next push_back into figure_ordered_subpaths_ will consume.
        pending_connect_to_prev_ = true;
    }

    void SVGConverter::renderFigure()
    {
        if (figure_ordered_subpaths_.empty())
            return;

        closeOpenDefs();

        int fillStyle = current_style_.fillStyle();
        bool edgeVis = current_style_.edgeVisibility();

        if (debug_fill_logging_)
        {
            const Color &fc = current_style_.fillColor();
            std::cerr << "[svg] figure cmd=" << current_command_index_
                      << " interior=" << interiorStyleName(fillStyle)
                      << " edgeVis=" << (edgeVis ? 1 : 0)
                      << " fillColor=" << static_cast<int>(fc.r) << ","
                      << static_cast<int>(fc.g) << ","
                      << static_cast<int>(fc.b)
                      << " segments=" << figure_ordered_subpaths_.size() << "\n";
        }

        // Walk the unified ordered list of figure sub-primitives.
        // Spec ISO 8632-1 §7.2.4 says all subpaths in a BEGIN FIGURE block
        // become one closed region with implicit bridging lines, but in
        // practice CGMs (FIGURE07) include multiple disjoint regions in a
        // single block. To respect both, bridge only when the next subpath's
        // start matches the previous subpath's end within a viewbox-scaled
        // tolerance. Endpoints in well-formed figures match exactly; truly
        // disjoint subpaths fall back to a fresh M-started subpath.
        // FIGURE03 (4 arcs in one block whose endpoints don't chain) cannot
        // be reconstructed faithfully under either policy without spec-
        // violating heuristics; left as a known gap.
        // FIGURE rendering policy. Three modes are needed to match the
        // NIST/CGMOpen reference renderings across the static10 corpus:
        //
        //   Mode A — figure contains CONNECTING EDGE primitives:
        //     CONN-EDGE acts as a sub-loop terminator. Within each sub-loop
        //     bounded by CONN-EDGEs, primitives bridge implicitly (M→L). At
        //     each CONN-EDGE the current sub-loop closes with Z and a new
        //     one begins. Matches FIGURE03's two pill-capsule sub-loops.
        //
        //   Mode B — no CONN-EDGE, mix of open + intrinsically-closed
        //   primitives (RECT/CIRCLE/ELLIPSE/closed ARC):
        //     Each primitive is its own sub-loop. ISO §7.2.4 strictly says
        //     bridge, but the reference renderings of showcase figures
        //     (FIGURE07: 11 mixed primitives) treat them as independent.
        //     Following the renderings is the practical choice.
        //
        //   Mode C — no CONN-EDGE, all primitives open (POLYLINE, partial
        //   arc, BEZIER):
        //     Apply ISO §7.2.4 implicit-bridge so the figure forms one
        //     closed region. FIGURE02's two POLYLINEs become a closed
        //     quadrilateral via M→L bridging.
        auto isClosedSubpath = [](const FigureSubpath &sp) {
            for (auto it = sp.svg.rbegin(); it != sp.svg.rend(); ++it) {
                if (*it == ' ' || *it == '\t' || *it == '\n') continue;
                if (*it == 'Z' || *it == 'z') return true;
                break;
            }
            // POLYLINE primitives don't emit Z but can still be conceptually
            // closed if the author authored a path whose last point is the
            // first point (FIGURE04's inner triangle: 4 vertices with the
            // last equal to the first).
            double dx = sp.start.x() - sp.end.x();
            double dy = sp.start.y() - sp.end.y();
            return (dx * dx + dy * dy) < 1e-6;
        };

        bool figureHasConnectEdge = false;
        bool figureHasClosedPrim = false;
        for (const auto &subpath : figure_ordered_subpaths_)
        {
            if (subpath.connect_to_prev) figureHasConnectEdge = true;
            if (isClosedSubpath(subpath)) figureHasClosedPrim = true;
        }
        // Bridge within sub-loops in Mode A (CONN-EDGE present) and Mode C
        // (no CONN-EDGE, all open). Mode B (no CONN-EDGE, mixed closed
        // primitives — showcase figures like FIGURE07) keeps each primitive
        // as its own sub-loop.
        bool bridgeWithinSubloop = figureHasConnectEdge || !figureHasClosedPrim;

        std::ostringstream pathStream;
        bool haveCurrent = false;
        bool subloopOpen = false;
        for (const auto &subpath : figure_ordered_subpaths_)
        {
            if (subpath.svg.empty()) continue;
            if (!haveCurrent)
            {
                pathStream << subpath.svg;
                haveCurrent = true;
                subloopOpen = true;
            }
            else if (figureHasConnectEdge && subpath.connect_to_prev)
            {
                if (subloopOpen) pathStream << " Z ";
                pathStream << subpath.svg;
                subloopOpen = true;
            }
            else if (bridgeWithinSubloop)
            {
                if (subpath.svg.size() >= 2 && subpath.svg[0] == 'M' &&
                    subpath.svg[1] == ' ')
                {
                    pathStream << "L " << subpath.svg.substr(2);
                }
                else
                {
                    pathStream << subpath.svg;
                }
            }
            else
            {
                // Mode B: each primitive its own sub-loop.
                if (subloopOpen) pathStream << " Z ";
                pathStream << subpath.svg;
                subloopOpen = true;
            }
        }

        if (subloopOpen) pathStream << " Z";
        std::string pathData = pathStream.str();
        if (pathData.empty())
        {
            return;
        }

        if (capturingProtectionRegionClip())
        {
            protection_region_paths_.push_back(pathData);
            return;
        }

        std::string fillAttr = getFillAttributeForCurrentStyle();
        // getFillAttributeForCurrentStyle() may have just emitted a <pattern>
        // into <defs>. Close defs again so the figure's <path> elements are
        // written outside the defs block and actually render.
        closeOpenDefs();

        std::string clipAttr = clipPathAttribute();
        std::string debugAttr = debugCommandAttribute();

        // SVG-idiomatic split for FIGURE rendering:
        //   1. ONE consolidated path with fill="..." stroke="none" — uses
        //      fill-rule=evenodd over all subpaths joined into one `d`, so
        //      the figure's interior fills correctly across multiple
        //      sub-primitives.
        //   2. PER-SUBPATH paths with fill="none" + edge stroke — one SVG
        //      <path> per CGM primitive that contributed to the figure.
        // This preserves the visual (same fill + same per-primitive strokes
        // as the previous single-path emission, since SVG strokes are
        // computed per-subpath inside one `d` anyway) AND satisfies the
        // W8 primitive-count parity rule, which counts SVG shape elements
        // and previously saw N CGM primitives consolidated to 1 element.
        svg_output_ << "  <path d=\"" << pathData << "\" ";
        if (!clipAttr.empty()) svg_output_ << clipAttr;
        if (!debugAttr.empty()) svg_output_ << debugAttr;
        svg_output_ << " fill-rule=\"evenodd\" "
                    << fillAttr << "stroke=\"none\" />\n";

        bool hollowForcesEdge = (fillStyle == 0);
        if (edgeVis || hollowForcesEdge)
        {
            // Edge stroke policy follows reference NIST viewer behavior:
            //
            // - CONN_EDGE present (FIGURE03): bridges between primitives
            //   are explicit per ISO §7.6.21, so the edge follows the
            //   consolidated path including bridges and per-sub-loop Z.
            // - No CONN_EDGE (FIGURE01/02/04/06/07): bridges are implicit
            //   for fill only, NOT drawn as edges. Each primitive's outline
            //   is drawn separately, with Z appended so single-primitive
            //   figures (FIGURE01 triangle, FIGURE02 quadrilateral via two
            //   POLYLINEs each appended with their own implicit close)
            //   render their closing edges. Polygons / closed shapes have
            //   trailing Z already; the appended Z is a no-op for them.
            std::string edgeStyle = current_style_.getEdgeStyle();
            if (figureHasConnectEdge)
            {
                svg_output_ << "  <path d=\"" << pathData << "\" ";
                if (!clipAttr.empty()) svg_output_ << clipAttr;
                svg_output_ << "fill=\"none\" " << edgeStyle << "/>\n";
            }
            else
            {
                for (const auto &subpath : figure_ordered_subpaths_)
                {
                    if (subpath.svg.empty()) continue;
                    svg_output_ << "  <path d=\"" << subpath.svg << " Z\" ";
                    if (!clipAttr.empty()) svg_output_ << clipAttr;
                    svg_output_ << "fill=\"none\" " << edgeStyle << "/>\n";
                }
            }
        }
    }

    void SVGConverter::processCircularArcCentre(CircularArcCentre *cmd)
    {
        if (!cmd)
            return;

        auto arc = svg::computeCenterBasedArc(
            cmd->center(), cmd->startDelta(), cmd->endDelta(), cmd->radius(),
            false /* not reversed */, picture_vdc_x_left_, picture_vdc_y_down_, transform_);

        // Inside BEGIN FIGURE / END FIGURE: arc subpath joins the figure's
        // accumulated path-data instead of emitting a standalone <path>.
        // Without this branch each arc segment of a closed figure becomes
        // its own open stroke-only path and the figure renders inverted /
        // unfilled. Mirrors the BeginFigure handling in processPolyBezier.
        if (in_figure_)
        {
            std::ostringstream frag;
            CGMPoint sp, ep;
            if (arc.is_full_circle) {
                // approximate full circle as two arcs in the figure path
                CGMPoint left(arc.center_svg.x() - arc.rx, arc.center_svg.y());
                CGMPoint right(arc.center_svg.x() + arc.rx, arc.center_svg.y());
                frag << "M " << left.x() << " " << left.y()
                     << " A " << arc.rx << " " << arc.ry << " 0 1 0 "
                     << right.x() << " " << right.y()
                     << " A " << arc.rx << " " << arc.ry << " 0 1 0 "
                     << left.x() << " " << left.y() << " ";
                sp = left; ep = left;
            } else {
                frag << "M " << arc.start_svg.x() << " " << arc.start_svg.y()
                     << " A " << arc.rx << " " << arc.ry
                     << " 0 " << arc.large_arc_flag << " " << arc.sweep_flag
                     << " " << arc.end_svg.x() << " " << arc.end_svg.y() << " ";
                sp = arc.start_svg; ep = arc.end_svg;
            }
            figure_path_fragments_.push_back(frag.str());
            figure_ordered_subpaths_.push_back({frag.str(), sp, ep, pending_connect_to_prev_});
            pending_connect_to_prev_ = false;
            return;
        }

        if (arc.is_full_circle) {
            svg_output_ << "  <ellipse cx=\"" << arc.center_svg.x()
                        << "\" cy=\"" << arc.center_svg.y()
                        << "\" rx=\"" << arc.rx
                        << "\" ry=\"" << arc.ry << "\" "
                        << current_style_.getStrokeStyle()
                        << "fill=\"none\" />\n";
            return;
        }

        svg_output_ << "  <path d=\"M " << arc.start_svg.x() << " " << arc.start_svg.y()
                    << " A " << arc.rx << " " << arc.ry
                    << " 0 " << arc.large_arc_flag << " " << arc.sweep_flag
                    << " " << arc.end_svg.x() << " " << arc.end_svg.y()
                    << "\" "
                    << current_style_.getStrokeStyle()
                    << "fill=\"none\" />\n";
    }

    void SVGConverter::processCircularArcCentreReversed(CircularArcCentreReversed *cmd)
    {
        if (!cmd)
            return;

        auto arc = svg::computeCenterBasedArc(
            cmd->center(), cmd->startDelta(), cmd->endDelta(), cmd->radius(),
            true /* reversed */, picture_vdc_x_left_, picture_vdc_y_down_, transform_);

        if (in_figure_)
        {
            std::ostringstream frag;
            CGMPoint sp, ep;
            if (arc.is_full_circle) {
                CGMPoint left(arc.center_svg.x() - arc.rx, arc.center_svg.y());
                CGMPoint right(arc.center_svg.x() + arc.rx, arc.center_svg.y());
                frag << "M " << left.x() << " " << left.y()
                     << " A " << arc.rx << " " << arc.ry << " 0 1 0 "
                     << right.x() << " " << right.y()
                     << " A " << arc.rx << " " << arc.ry << " 0 1 0 "
                     << left.x() << " " << left.y() << " ";
                sp = left; ep = left;
            } else {
                frag << "M " << arc.start_svg.x() << " " << arc.start_svg.y()
                     << " A " << arc.rx << " " << arc.ry
                     << " 0 " << arc.large_arc_flag << " " << arc.sweep_flag
                     << " " << arc.end_svg.x() << " " << arc.end_svg.y() << " ";
                sp = arc.start_svg; ep = arc.end_svg;
            }
            figure_path_fragments_.push_back(frag.str());
            figure_ordered_subpaths_.push_back({frag.str(), sp, ep, pending_connect_to_prev_});
            pending_connect_to_prev_ = false;
            return;
        }

        if (arc.is_full_circle) {
            svg_output_ << "  <ellipse cx=\"" << arc.center_svg.x()
                        << "\" cy=\"" << arc.center_svg.y()
                        << "\" rx=\"" << arc.rx
                        << "\" ry=\"" << arc.ry << "\" "
                        << current_style_.getStrokeStyle()
                        << "fill=\"none\" />\n";
            return;
        }

        svg_output_ << "  <path d=\"M " << arc.start_svg.x() << " " << arc.start_svg.y()
                    << " A " << arc.rx << " " << arc.ry
                    << " 0 " << arc.large_arc_flag << " " << arc.sweep_flag
                    << " " << arc.end_svg.x() << " " << arc.end_svg.y()
                    << "\" "
                    << current_style_.getStrokeStyle()
                    << "fill=\"none\" />\n";
    }

    void SVGConverter::processCircularArc3Point(CircularArc3Point *cmd)
    {
        if (!cmd)
            return;

        CGMPoint p1 = cmd->start();
        CGMPoint p2 = cmd->intermediate();
        CGMPoint p3 = cmd->end();

        auto arc_opt = svg::compute3PointArc(p1, p2, p3,
            picture_vdc_x_left_, picture_vdc_y_down_, transform_);

        if (in_figure_)
        {
            std::ostringstream frag;
            CGMPoint sp, ep;
            if (!arc_opt) {
                CGMPoint start_svg = transform_.transformPoint(p1);
                CGMPoint end_svg = transform_.transformPoint(p3);
                frag << "M " << start_svg.x() << " " << start_svg.y()
                     << " L " << end_svg.x() << " " << end_svg.y() << " ";
                sp = start_svg; ep = end_svg;
            } else if (arc_opt->is_full_circle) {
                const auto& arc = *arc_opt;
                CGMPoint left(arc.center_svg.x() - arc.rx, arc.center_svg.y());
                CGMPoint right(arc.center_svg.x() + arc.rx, arc.center_svg.y());
                frag << "M " << left.x() << " " << left.y()
                     << " A " << arc.rx << " " << arc.ry << " 0 1 0 "
                     << right.x() << " " << right.y()
                     << " A " << arc.rx << " " << arc.ry << " 0 1 0 "
                     << left.x() << " " << left.y() << " ";
                sp = left; ep = left;
            } else {
                const auto& arc = *arc_opt;
                frag << "M " << arc.start_svg.x() << " " << arc.start_svg.y()
                     << " A " << arc.rx << " " << arc.ry
                     << " 0 " << arc.large_arc_flag << " " << arc.sweep_flag
                     << " " << arc.end_svg.x() << " " << arc.end_svg.y() << " ";
                sp = arc.start_svg; ep = arc.end_svg;
            }
            figure_path_fragments_.push_back(frag.str());
            figure_ordered_subpaths_.push_back({frag.str(), sp, ep, pending_connect_to_prev_});
            pending_connect_to_prev_ = false;
            return;
        }

        if (!arc_opt) {
            // Points are collinear, fall back to straight line
            CGMPoint start_svg = transform_.transformPoint(p1);
            CGMPoint end_svg = transform_.transformPoint(p3);
            svg_output_ << "  <path d=\"M " << start_svg.x() << " " << start_svg.y()
                        << " L " << end_svg.x() << " " << end_svg.y()
                        << "\" " << current_style_.getStrokeStyle()
                        << "fill=\"none\" />\n";
            return;
        }

        auto& arc = *arc_opt;

        if (arc.is_full_circle) {
            svg_output_ << "  <ellipse cx=\"" << arc.center_svg.x()
                        << "\" cy=\"" << arc.center_svg.y()
                        << "\" rx=\"" << arc.rx
                        << "\" ry=\"" << arc.ry << "\" "
                        << current_style_.getStrokeStyle()
                        << "fill=\"none\" />\n";
            return;
        }

        svg_output_ << "  <path d=\"M " << arc.start_svg.x() << " " << arc.start_svg.y()
                    << " A " << arc.rx << " " << arc.ry
                    << " 0 " << arc.large_arc_flag << " " << arc.sweep_flag
                    << " " << arc.end_svg.x() << " " << arc.end_svg.y()
                    << "\" "
                    << current_style_.getStrokeStyle()
                    << "fill=\"none\" />\n";
    }

    void SVGConverter::processCircularArc3PointClose(CircularArc3PointClose *cmd)
    {
        if (!cmd)
            return;

        CGMPoint p1 = cmd->start();
        CGMPoint p2 = cmd->intermediate();
        CGMPoint p3 = cmd->end();

        auto arc_opt = svg::compute3PointArc(
            p1, p2, p3,
            picture_vdc_x_left_, picture_vdc_y_down_,
            transform_);

        if (!arc_opt) {
            // Collinear points - draw a closed line
            CGMPoint start_svg = transform_.transformPoint(p1);
            CGMPoint end_svg = transform_.transformPoint(p3);
            svg_output_ << "  <path d=\"M " << start_svg.x() << " " << start_svg.y()
                        << " L " << end_svg.x() << " " << end_svg.y();
            if (cmd->closure() == 0) {
                CGMPoint mid_vdc((p1.x() + p3.x()) / 2.0, (p1.y() + p3.y()) / 2.0);
                CGMPoint mid_svg = transform_.transformPoint(mid_vdc);
                svg_output_ << " L " << mid_svg.x() << " " << mid_svg.y() << " Z";
            } else {
                svg_output_ << " Z";
            }
            svg_output_ << "\" " << current_style_.getStyleWithEdges() << "/>\n";
            return;
        }

        const auto& arc = *arc_opt;

        if (arc.is_full_circle) {
            svg_output_ << "  <ellipse cx=\"" << arc.center_svg.x()
                        << "\" cy=\"" << arc.center_svg.y()
                        << "\" rx=\"" << arc.rx
                        << "\" ry=\"" << arc.ry << "\" "
                        << current_style_.getStyleWithEdges()
                        << "/>\n";
            return;
        }

        svg_output_ << "  <path d=\"M " << arc.start_svg.x() << " " << arc.start_svg.y()
                    << " A " << arc.rx << " " << arc.ry
                    << " 0 " << arc.large_arc_flag << " " << arc.sweep_flag
                    << " " << arc.end_svg.x() << " " << arc.end_svg.y();

        if (cmd->closure() == 0) {
            svg_output_ << " L " << arc.center_svg.x() << " " << arc.center_svg.y() << " Z";
        } else {
            svg_output_ << " Z";
        }

        svg_output_ << "\" " << current_style_.getStyleWithEdges() << "/>\n";
    }

    void SVGConverter::processCircularArcCentreClose(CircularArcCentreClose *cmd)
    {
        if (!cmd)
            return;

        auto arc = svg::computeCenterBasedArc(
            cmd->center(),
            cmd->startDelta(),
            cmd->endDelta(),
            cmd->radius(),
            false,  // not reversed (CCW)
            picture_vdc_x_left_, picture_vdc_y_down_,
            transform_);

        if (arc.is_full_circle) {
            svg_output_ << "  <ellipse cx=\"" << arc.center_svg.x()
                        << "\" cy=\"" << arc.center_svg.y()
                        << "\" rx=\"" << arc.rx
                        << "\" ry=\"" << arc.ry << "\" "
                        << current_style_.getStyleWithEdges()
                        << "/>\n";
            return;
        }

        svg_output_ << "  <path d=\"M " << arc.start_svg.x() << " " << arc.start_svg.y()
                    << " A " << arc.rx << " " << arc.ry
                    << " 0 " << arc.large_arc_flag << " " << arc.sweep_flag
                    << " " << arc.end_svg.x() << " " << arc.end_svg.y();

        if (cmd->closure() == 0) {
            svg_output_ << " L " << arc.center_svg.x() << " " << arc.center_svg.y() << " Z";
        } else {
            svg_output_ << " Z";
        }

        svg_output_ << "\" " << current_style_.getStyleWithEdges() << "/>\n";
    }

    void SVGConverter::processTextColor(TextColour *cmd)
    {
        if (!cmd)
            return;

        Color resolved = resolveColor(cmd->color(), ColorRole::Text, "TEXT COLOUR");
        if (cmd->color().isIndexed()) {
            text_color_slots_.indexed = resolved;
        } else {
            text_color_slots_.direct = resolved;
        }
        current_style_.setTextColor(resolved);
        current_style_.markTextColorExplicit();
    }

    void SVGConverter::processCharacterHeight(CharacterHeight *cmd)
    {
        if (!cmd)
            return;

        // Store original CHARACTER HEIGHT for RESTRICTED TEXT
        // (RESTRICTED TEXT extent is designed for CGM CHARACTER HEIGHT semantics)
        current_style_.setOriginalTextHeight(cmd->height());

        // Common CGM viewer practice (and what WebCGM reference renderings
        // assume) is to treat CHARACTER HEIGHT as the nominal character body
        // size — i.e., directly the SVG font-size em-square. The prior
        // cap-height-to-em-square /0.7 expansion produced text ~1.43x larger
        // than every WebCGM 2.1 test's reference PNG expected, and the
        // cap-height reading of the spec is not what real viewers implement.
        current_style_.setTextHeight(cmd->height());
        current_style_.markCharacterHeightExplicit();
    }

    void SVGConverter::processCharacterOrientation(CharacterOrientation *cmd)
    {
        if (!cmd)
        {
            return;
        }

        // ISO/IEC 8632-1 §7.7.16 defines the parameter order as (character up vector, character base vector).
        // The Binary CGM command stores the first vector in xUp() and the second in yUp(), so map accordingly.
        character_orientation_up_ = cmd->xUp();
        character_orientation_base_ = cmd->yUp();

        if (std::fabs(character_orientation_base_.x()) < 1e-9 && std::fabs(character_orientation_base_.y()) < 1e-9)
        {
            character_orientation_base_ = CGMPoint(1.0, 0.0);
        }
        if (std::fabs(character_orientation_up_.x()) < 1e-9 && std::fabs(character_orientation_up_.y()) < 1e-9)
        {
            character_orientation_up_ = CGMPoint(0.0, 1.0);
        }
    }

    void SVGConverter::processTextAlignment(TextAlignment *cmd)
    {
        if (!cmd)
            return;

        current_style_.setTextAlignment(
            cmd->horizontalAlignment(),
            cmd->verticalAlignment());
    }

    void SVGConverter::processPolyBezier(PolyBezier *cmd)
    {
        if (!cmd || cmd->controlPoints().empty())
            return;

        const auto &points = cmd->controlPoints();

        int continuity = cmd->continuityIndicator();
        size_t i = 0;

        auto emitBezier = [&](std::ostream &out)
        {
            if (continuity == 1)
            {
                while (i + 3 < points.size())
                {
                    CGMPoint p0 = transform_.transformPoint(points[i]);
                    CGMPoint p1 = transform_.transformPoint(points[i + 1]);
                    CGMPoint p2 = transform_.transformPoint(points[i + 2]);
                    CGMPoint p3 = transform_.transformPoint(points[i + 3]);

                    out << "M " << p0.x() << " " << p0.y() << " ";
                    out << "C " << p1.x() << " " << p1.y() << " "
                        << p2.x() << " " << p2.y() << " "
                        << p3.x() << " " << p3.y() << " ";

                    i += 4;
                }
            }
            else
            {
                if (points.size() < 4)
                    return;

                CGMPoint start = transform_.transformPoint(points[0]);
                out << "M " << start.x() << " " << start.y() << " ";

                i = 1;
                while (i + 2 < points.size())
                {
                    CGMPoint p1 = transform_.transformPoint(points[i]);
                    CGMPoint p2 = transform_.transformPoint(points[i + 1]);
                    CGMPoint p3 = transform_.transformPoint(points[i + 2]);

                    out << "C " << p1.x() << " " << p1.y() << " "
                        << p2.x() << " " << p2.y() << " "
                        << p3.x() << " " << p3.y() << " ";

                    i += 3;
                }
            }
        };

        if (in_figure_)
        {
            std::ostringstream fragment;
            emitBezier(fragment);
            figure_path_fragments_.push_back(fragment.str());
            // Approximate bezier start/end as first and last transformed control
            // points. Good enough for endpoint-bridging logic in renderFigure;
            // exact bezier endpoints would require tracking inside emitBezier.
            CGMPoint sp = points.empty() ? CGMPoint(0.0, 0.0) : transform_.transformPoint(points.front());
            CGMPoint ep = points.empty() ? CGMPoint(0.0, 0.0) : transform_.transformPoint(points.back());
            figure_ordered_subpaths_.push_back({fragment.str(), sp, ep, pending_connect_to_prev_});
            pending_connect_to_prev_ = false;
            if (debug_fill_logging_)
            {
                std::cerr << "[svg] figure bezier cmd=" << current_command_index_
                          << " fragmentLen=" << figure_path_fragments_.back().size() << "\n";
            }
            return;
        }

        svg_output_ << "  <path d=\"";
        emitBezier(svg_output_);
        svg_output_ << "\" "
                    << current_style_.getStrokeStyle()
                    << "fill=\"none\"" << debugCommandAttribute() << " />\n";
    }

    void SVGConverter::processCellArray(CellArray *cmd)
    {
        if (!cmd)
            return;

        const auto &colorArray = cmd->colorArray();
        int nx = cmd->nx();
        int ny = cmd->ny();

        if (nx <= 0 || ny <= 0 || colorArray.empty())
        {
            return;
        }

        std::vector<std::vector<Color>> resolvedRows(
            std::min(
                static_cast<size_t>(ny),
                colorArray.size()));
        const bool applyTransparency =
            transparent_cell_active_ &&
            transparent_cell_colour_enabled_;

        std::set<int> referencedColorIndices;

        if (color_logging_enabled_)
        {
            std::cerr << "[raster] cell array precision=" << cmd->localColorPrecision() << "\n";
        }

        for (int y = 0; y < ny; ++y)
        {
            const std::vector<CGMColor> *rowPtr = (y < static_cast<int>(colorArray.size())) ? &colorArray[y] : nullptr;
            if (rowPtr &&
                y < static_cast<int>(resolvedRows.size()))
            {
                resolvedRows[static_cast<size_t>(y)].reserve(
                    std::min(
                        static_cast<size_t>(nx),
                        rowPtr->size()));
            }
            for (int x = 0; x < nx; ++x)
            {
                if (rowPtr && x < static_cast<int>(rowPtr->size()))
                {
                    const CGMColor &raw = (*rowPtr)[x];
                    if (color_logging_enabled_)
                    {
                        if (raw.isIndexed())
                        {
                            std::cerr << "[raster] cell(" << x << "," << y << ") index=" << raw.colorIndex() << "\n";
                        }
                        else
                        {
                            Color direct = raw.color();
                            std::cerr << "[raster] cell(" << x << "," << y << ") direct="
                                      << static_cast<int>(direct.r) << ","
                                      << static_cast<int>(direct.g) << ","
                                      << static_cast<int>(direct.b) << "\n";
                        }
                    }
                    if (color_logging_enabled_ && raw.isIndexed())
                    {
                        referencedColorIndices.insert(raw.colorIndex());
                    }

                    Color resolved = resolveColor(raw, ColorRole::Raster);
                    if (color_logging_enabled_)
                    {
                        std::cerr << "    -> resolved rgb(" << static_cast<int>(resolved.r) << ","
                                  << static_cast<int>(resolved.g) << ","
                                  << static_cast<int>(resolved.b) << ")\n";
                    }
                    resolvedRows[static_cast<size_t>(y)]
                        .push_back(resolved);
                }
            }
        }

        svg::CellArrayPixelsInput pixelsInput;
        pixelsInput.width = nx;
        pixelsInput.height = ny;
        pixelsInput.resolved_rows = std::move(resolvedRows);
        pixelsInput.apply_transparency = applyTransparency;
        pixelsInput.transparent_color =
            transparent_cell_color_;
        auto preparedPixels =
            svg::CellArrayPreparer::preparePixels(
                std::move(pixelsInput));
        if (!preparedPixels.valid)
        {
            return;
        }

        auto colors = std::move(preparedPixels.colors);
        const size_t transparentPixels =
            preparedPixels.transparent_pixels;
        const size_t opaquePixels =
            preparedPixels.opaque_pixels;
        const size_t totalPixels =
            preparedPixels.total_pixels;

        std::string base64Data;
        std::string mimeType;

#ifdef _WIN32
        // Determine encoding format based on settings
        RasterEncoding effectiveEncoding = raster_encoding_;
        if (effectiveEncoding == RasterEncoding::Auto)
        {
            effectiveEncoding = detectOptimalEncoding(colors, nx, ny);
        }

        // Encode based on selected format
        if (effectiveEncoding == RasterEncoding::JPEG)
        {
            // JPEG encoding - only for non-transparent images
            // If transparency is needed, fall back to PNG
            if (transparentPixels == 0)
            {
                std::string jpegData = encodeCellArrayToJpeg(colors, nx, ny, jpeg_quality_);
                if (!jpegData.empty())
                {
                    base64Data = std::move(jpegData);
                    mimeType = "image/jpeg";
                }
            }
        }

        // Use PNG if JPEG wasn't selected or failed, or if transparency is needed
        if (base64Data.empty())
        {
            std::string pngData = encodeCellArrayToPng(colors, nx, ny);
            if (!pngData.empty())
            {
                base64Data = std::move(pngData);
                mimeType = "image/png";
            }
        }
#endif
        // Fallback to BMP if all else fails
        if (base64Data.empty())
        {
            base64Data = encodeCellArrayToBmp(colors, nx, ny);
            mimeType = "image/bmp";
        }
        if (base64Data.empty())
        {
            return;
        }

        if (color_logging_enabled_ && !referencedColorIndices.empty())
        {
            std::cerr << "[color-table] raster indices:";
            for (int idx : referencedColorIndices)
            {
                std::cerr << " " << idx;
            }
            std::cerr << "\n";
        }

        // CELL ARRAY corner semantics:
        // P = first corner (origin)
        // R = end of first row (P->R is row/X direction)
        // Q = diagonal corner (end of last row)
        // Column direction = Q - R (NOT Q - P which is diagonal)
        CGMPoint origin = cmd->cornerP();
        CGMPoint rowVector = subtractPoints(cmd->cornerR(), origin);         // P->R: row direction (X)
        CGMPoint colVector = subtractPoints(cmd->cornerQ(), cmd->cornerR()); // R->Q: column direction (Y)
        // Row and column vectors become the image width and height bases.
        CGMPoint pathVector = rowVector;
        CGMPoint lineVector = colVector;

        const double widthUnits = static_cast<double>(nx);
        const double heightUnits = static_cast<double>(ny);
        const CGMPoint originSvg =
            transform_.transformPoint(origin);
        const CGMPoint pathSvgEnd =
            transform_.transformPoint(
                addPoints(origin, pathVector));
        const CGMPoint lineSvgEnd =
            transform_.transformPoint(
                addPoints(origin, lineVector));
        const CGMPoint diagonalSvgEnd =
            transform_.transformPoint(
                addPoints(
                    addPoints(origin, pathVector),
                    lineVector));

        svg::CellArrayPlacementInput placementInput;
        placementInput.pixel_width = nx;
        placementInput.pixel_height = ny;
        placementInput.origin = {
            originSvg.x(),
            originSvg.y()};
        placementInput.row_end = {
            pathSvgEnd.x(),
            pathSvgEnd.y()};
        placementInput.column_end = {
            lineSvgEnd.x(),
            lineSvgEnd.y()};
        placementInput.diagonal = {
            diagonalSvgEnd.x(),
            diagonalSvgEnd.y()};
        const auto placement =
            svg::CellArrayPreparer::resolvePlacement(
                placementInput);

        if (debug_fill_logging_)
        {
            // Log RAW VDC coordinates (before transformation)
            std::cerr << "[svg] CellArray RAW VDC corners:\n";
            std::cerr << "[svg]   P(" << origin.x() << "," << origin.y() << ")\n";
            std::cerr << "[svg]   Q(" << cmd->cornerQ().x() << "," << cmd->cornerQ().y() << ")\n";
            std::cerr << "[svg]   R(" << cmd->cornerR().x() << "," << cmd->cornerR().y() << ")\n";
            std::cerr << "[svg]   rowVector(" << rowVector.x() << "," << rowVector.y() << ")\n";
            std::cerr << "[svg]   colVector(" << colVector.x() << "," << colVector.y() << ")\n";
            std::cerr << "[svg]   nx=" << nx << " ny=" << ny << "\n";

            // Log VDC extent for comparison
            double vdcX1, vdcY1, vdcX2, vdcY2;
            transform_.getVdcExtent(vdcX1, vdcY1, vdcX2, vdcY2);
            std::cerr << "[svg]   VDC extent: (" << vdcX1 << "," << vdcY1
                      << ") to (" << vdcX2 << "," << vdcY2 << ")\n";

            // Log transformed SVG coordinates
            std::cerr << "[svg] CellArray SVG corners P("
                      << originSvg.x() << "," << originSvg.y() << ") R("
                      << pathSvgEnd.x() << "," << pathSvgEnd.y() << ") P+col("
                      << lineSvgEnd.x() << "," << lineSvgEnd.y() << ") Q("
                      << diagonalSvgEnd.x() << ","
                      << diagonalSvgEnd.y() << ")\n";
            std::cerr << "[svg] CellArray pixels=" << widthUnits << "x" << heightUnits << "\n";
        }

        std::string dataUri = "data:" + mimeType + ";base64," + base64Data;

        bool logRaster = raster_logging_enabled_;
        RasterMetrics metrics;
        if (logRaster)
        {
            metrics.kind = RasterMetrics::Kind::CellArray;
            metrics.picture_index = current_picture_index_;
            metrics.command_index = current_command_index_;
            metrics.pixel_width = nx;
            metrics.pixel_height = ny;
            metrics.transparent_pixels = transparentPixels;
            metrics.opaque_pixels = opaquePixels;
            metrics.tcc_active = applyTransparency;
            metrics.had_tcc_match = applyTransparency && transparentPixels > 0;
            metrics.mime_type = mimeType;
            metrics.compression = "cellarray";
            metrics.origin = origin;
            metrics.path_vector = pathVector;
            metrics.line_vector = lineVector;
        }

        const auto &matrix = placement.matrix;
        const bool matrixValid = placement.use_matrix;

        if (debug_fill_logging_)
        {
            std::cerr << "[svg] CellArray matrix: a=" << matrix.a << " b=" << matrix.b
                      << " c=" << matrix.c << " d=" << matrix.d
                      << " e=" << matrix.e << " f=" << matrix.f
                      << " valid=" << (matrixValid ? "yes" : "no") << "\n";
        }

        if (matrixValid)
        {
            std::ostringstream transformAttr;
            transformAttr << "matrix(" << matrix.a << "," << matrix.b << ","
                          << matrix.c << "," << matrix.d << ","
                          << matrix.e << "," << matrix.f << ")";

            svg_output_ << "  <image x=\"0\" y=\"0\" width=\"" << widthUnits
                        << "\" height=\"" << heightUnits
                        << "\" style=\"image-rendering:pixelated\" "
                        << "transform=\"" << transformAttr.str() << "\" "
                        << clipPathAttribute();
            svg_output_ << "xlink:href=\"" << dataUri << "\" href=\"" << dataUri << "\" ";
            svg_output_ << "preserveAspectRatio=\"none\"" << debugCommandAttribute() << " />\n";
            if (logRaster)
            {
                metrics.has_transform = true;
                metrics.transform[0] = matrix.a;
                metrics.transform[1] = matrix.b;
                metrics.transform[2] = matrix.c;
                metrics.transform[3] = matrix.d;
                metrics.transform[4] = matrix.e;
                metrics.transform[5] = matrix.f;
                raster_metrics_.push_back(metrics);
                std::cerr << "[raster] cell-array " << nx << "x" << ny
                          << " transparent=" << transparentPixels << "/" << totalPixels
                          << " matrix" << (metrics.has_transform ? "=yes" : "=no") << "\n";
                if (metrics.tcc_active && !metrics.had_tcc_match)
                {
                    std::cerr << "[raster] cell-array warning: TransparentCellColour defined but no pixels matched\n";
                }
            }
            return;
        }

        if (!placement.fallback_valid)
        {
            return;
        }

        svg_output_ << "  <image x=\"" << placement.fallback_x
                    << "\" y=\"" << placement.fallback_y
                    << "\" width=\"" << placement.fallback_width
                    << "\" height=\"" << placement.fallback_height
                    << "\" style=\"image-rendering:pixelated\" "
                    << clipPathAttribute();
        svg_output_ << "xlink:href=\"" << dataUri << "\" href=\"" << dataUri << "\" ";
        svg_output_ << "preserveAspectRatio=\"none\"" << debugCommandAttribute() << " />\n";

        if (logRaster)
        {
            metrics.mime_type = mimeType;
            metrics.has_transform = false;
            metrics.transform[0] = metrics.transform[1] = metrics.transform[2] = metrics.transform[3] = metrics.transform[4] = metrics.transform[5] = 0.0;
            raster_metrics_.push_back(metrics);
            std::cerr << "[raster] cell-array " << nx << "x" << ny
                      << " transparent=" << transparentPixels << "/" << totalPixels
                      << " matrix=no" << "\n";
            if (metrics.tcc_active && !metrics.had_tcc_match)
            {
                std::cerr << "[raster] cell-array warning: TransparentCellColour defined but no pixels matched\n";
            }
        }
    }

    void SVGConverter::processBeginTileArray(BeginTileArray *cmd)
    {
        if (!cmd)
            return;

        in_tile_array_ = true;
        tile_array_position_ = cmd->position();
        tile_path_direction_ = cmd->cellPathDirection();
        tile_line_direction_ = cmd->lineProgressionDirection();
        tile_tiles_in_path_ = std::max(1, cmd->nTilesInPathDirection());
        tile_tiles_in_line_ = std::max(1, cmd->nTilesInLineDirection());
        tile_cells_per_tile_path_ = std::max(1, cmd->nCellsPerTileInPathDirection());
        tile_cells_per_tile_line_ = std::max(1, cmd->nCellsPerTileInLineDirection());
        tile_image_offset_path_ = std::max(0, cmd->imageOffsetInPathDirection());
        tile_image_offset_line_ = std::max(0, cmd->imageOffsetInLineDirection());
        tile_image_cells_path_ = cmd->imageCellsInPathDirection();
        tile_image_cells_line_ = cmd->imageCellsInLineDirection();
        tile_current_index_ = 0;
        tile_cell_width_ = cmd->cellSizeInPathDirection();
        tile_cell_height_ = cmd->cellSizeInLineDirection();

        // No metric_scale_factor application here. cellSize is type R (REAL)
        // in VDC space (ISO 8632-1 §5.4.5); SVG output also stays in VDC,
        // so converting through metric units would inflate by 1/metric.
        // The previous divide was added when cellSize was being parsed as
        // 16-bit integer — a parse bug now fixed in BeginTileArray.

        if (!std::isfinite(tile_cell_width_))
        {
            tile_cell_width_ = 0.0;
        }
        if (!std::isfinite(tile_cell_height_))
        {
            tile_cell_height_ = 0.0;
        }

        if (debug_fill_logging_)
        {
            std::cerr << "[svg] BeginTileArray pos=(" << tile_array_position_.x() << "," << tile_array_position_.y()
                      << ") tiles=" << tile_tiles_in_path_ << "x" << tile_tiles_in_line_
                      << " cellSize=(" << tile_cell_width_ << "," << tile_cell_height_ << ")"
                      << " cellsPerTile=(" << tile_cells_per_tile_path_ << "," << tile_cells_per_tile_line_ << ")"
                      << " offsets=(" << tile_image_offset_path_ << "," << tile_image_offset_line_ << ")"
                      << " imageCells=(" << tile_image_cells_path_ << "," << tile_image_cells_line_ << ")\n";
        }
    }

    void SVGConverter::processEndTileArray(EndTileArray *cmd)
    {
        (void)cmd;
        resetTileArrayState();
    }

    void SVGConverter::processBeginPicture(BeginPicture *cmd)
    {
        flushPendingText();
        resetPictureState();
        ++current_picture_index_;

        if (current_picture_index_ > 0)
        {
            svg_output_.str("");
            svg_output_.clear();
            defs_open_ = false;
            pattern_counter_ = 0;
            symbol_placeholder_counter_ = 0;
            gdp_placeholder_counter_ = 0;
            symbol_definition_ids_.clear();
            emitted_symbol_defs_.clear();
            symbol_definition_sources_.clear();
            symbol_definition_fragments_.clear();
            aps_id_allocator_.reset();
            aps_metadata_entries_.clear();
            clip_path_counter_ = 0;
            writeSvgHeader();
        }

        if (cmd && !cmd->name().empty())
        {
            current_picture_name_ = utils::trimString(cmd->name());
        }
        else
        {
            current_picture_name_.clear();
        }

        if (color_logging_enabled_)
        {
            std::cerr << "[picture] begin index=" << current_picture_index_;
            if (cmd != nullptr && !cmd->name().empty())
            {
                std::cerr << " name=\"" << cmd->name() << "\"";
            }
            std::cerr << "\n";
        }
    }

    void SVGConverter::processBeginPictureBody(BeginPictureBody *cmd)
    {
        (void)cmd;

        // If the CGM prelude did not set CHARACTER HEIGHT explicitly, the ISO
        // 8632-1 spec leaves the numeric default to the profile. The engine's
        // initializer value of 12.0 VDC units is fine for small-extent CGMs
        // but renders as sub-pixel text when the picture extent is in the
        // thousands (a common WebCGM case). Pick a VDC-extent-relative
        // default that produces ~5% of picture height, close to what real
        // WebCGM viewers use for "default text size". This only kicks in for
        // CGMs that never emit element 5.16 CHARACTER HEIGHT.
        if (!current_style_.characterHeightExplicit() && picture_vdc_height_ > 400.0)
        {
            double defaultHeight = picture_vdc_height_ * 0.025;
            current_style_.setOriginalTextHeight(defaultHeight);
            current_style_.setTextHeight(defaultHeight);
        }

        // SVGStyle's font_family_ defaults to "Arial, Helvetica, sans-serif"
        // — independent of what the CGM declared in FONT LIST. Reference
        // viewers default to FONT LIST entry 1 when no TEXT FONT INDEX
        // has been emitted yet, so CELARY01's "WebCGM Ed. 1.0" label
        // (which inherits the default index) renders as Arial here but as
        // HELVETICA-BOLD (entry 1) in the reference. Resolve the current
        // index against FONT LIST so the default tracks the metafile.
        if (!font_list_.empty())
        {
            current_style_.setFontFamily(
                resolveFontFamilyFromIndex(current_style_.fontIndex()));
        }

        // SVGStyle's constructor defaults line_width_/edge_width_ to 1.0
        // (1 SVG unit). For CGMs that never emit LINE/EDGE WIDTH, that
        // default is sub-pixel against typical 20000-unit viewBoxes
        // (CHRSET01's grid lines were invisible at 491px raster).
        // computeStrokeWidth() applies a viewport-min safety net for
        // explicit widths; mirror that here for the constructor default
        // by clamping to viewbox_longest/2000.
        double viewbox_w = svg_bounds_x2_ - svg_bounds_x1_;
        double viewbox_h = svg_bounds_y2_ - svg_bounds_y1_;
        double viewport_min = std::max(viewbox_w, viewbox_h) / 2000.0;
        if (viewport_min > 0.0)
        {
            if (current_style_.lineWidth() < viewport_min)
            {
                current_style_.setLineWidth(viewport_min);
            }
            if (current_style_.edgeWidth() < viewport_min)
            {
                current_style_.setEdgeWidth(viewport_min);
            }
        }

        // BACKGROUND COLOUR canvas paint (ISO 8632-1 §6.4.7).
        // Reference WebCGM 1.0 renderers paint the canvas with the
        // resolved colour-index-0 value before any primitives. BG COLOUR
        // sets index 0; subsequent COLR-TABLE entries can override it
        // (BGCOLR04: BG=black then COLR-TABLE resets index 0 to white).
        //
        // Painting with color_table_[0] at picture body start handles
        // both halves automatically: ESCAPE01 paints cyan (BG sets
        // index 0=cyan, no override), BGCOLR02 paints black (BG=black,
        // no override), BGCOLR04 paints nothing (override resets to
        // white = SVG default), TABTNL01 paints nothing (BG=white via
        // CVE-rescaled 16-bit values).
        if (background_color_explicit_)
        {
            auto it = color_table_.find(0);
            if (it != color_table_.end())
            {
                const Color &bg = it->second;
                bool isWhite = (bg.r == 255 && bg.g == 255 && bg.b == 255);
                if (!isWhite)
                {
                    char hex[8];
                    std::snprintf(hex, sizeof(hex), "#%02X%02X%02X",
                                  static_cast<int>(bg.r),
                                  static_cast<int>(bg.g),
                                  static_cast<int>(bg.b));
                    svg_output_ << "  <rect x=\"" << svg_bounds_x1_
                                << "\" y=\"" << svg_bounds_y1_
                                << "\" width=\"" << viewbox_w
                                << "\" height=\"" << viewbox_h
                                << "\" fill=\"" << hex
                                << "\" stroke=\"none\" data-cgm-bg=\"1\" />\n";
                }
            }
        }

        // ISO 8632-1 §A.1: in INDEXED colour-selection mode the default
        // LINE/FILL/EDGE/TEXT/MARKER COLOUR is colour-index-1. Refresh
        // non-explicit colour fields from color_table_[1] so primitives
        // drawn before any explicit *_COLOUR command pick up the
        // foreground colour set by COLR-TABLE in the picture descriptor.
        // Subsequent COLR-TABLE updates of index 1 also re-trigger this
        // (see processColourTable).
        refreshDefaultIndexedColors();
    }

    void SVGConverter::processEndPicture(EndPicture *cmd)
    {
        (void)cmd;
        flushPendingText();
        in_protection_region_ = false;
        protection_region_paths_.clear();
        protection_region_definitions_.clear();
        protection_region_clip_ids_.clear();
        clip_enabled_ = false;
        clip_path_attribute_.clear();
        resetTileArrayState();
        transparent_cell_active_ = false;
        transparent_cell_color_ = Color::White();
    }

    SVGConverter::TileGeometry SVGConverter::computeTileGeometry(
        const TileElement &tile,
        bool applyMalformedSizeHeuristics)
    {
        svg::TileGeometryInput input;
        input.preferred_width = tile.bitmapWidth();
        input.preferred_height = tile.bitmapHeight();
        input.image_cells_path = tile_image_cells_path_;
        input.image_cells_line = tile_image_cells_line_;
        input.cells_per_tile_path = tile_cells_per_tile_path_;
        input.cells_per_tile_line = tile_cells_per_tile_line_;
        input.picture_width = picture_vdc_width_;
        input.picture_height = picture_vdc_height_;
        input.picture_width_raw = picture_vdc_width_raw_;
        input.picture_height_raw = picture_vdc_height_raw_;
        input.viewbox_width = svg_viewbox_width_;
        input.viewbox_height = svg_viewbox_height_;
        input.cell_size_path = tile_cell_width_;
        input.cell_size_line = tile_cell_height_;
        input.apply_malformed_size_heuristics =
            applyMalformedSizeHeuristics;
        input.in_tile_array = in_tile_array_;
        input.tile_index = tile_current_index_;
        input.tiles_in_path = tile_tiles_in_path_;
        input.image_offset_path = tile_image_offset_path_;
        input.image_offset_line = tile_image_offset_line_;
        input.array_position = tile_array_position_;
        input.path_direction = tile_path_direction_;
        input.line_direction = tile_line_direction_;
        input.flip_y = transform_.getFlipY();

        TileGeometry geometry =
            svg::TileGeometryResolver::resolve(input);
        if (applyMalformedSizeHeuristics)
        {
            if (geometry.invalidWidthCellSize)
            {
                std::cerr
                    << "[WARN] tile width per cell invalid; "
                       "using viewBox-derived fallback\n";
            }
            if (geometry.invalidHeightCellSize)
            {
                std::cerr
                    << "[WARN] tile height per cell invalid; "
                       "using viewBox-derived fallback\n";
            }
            if (geometry.usedWidthHeuristic)
            {
                std::cerr
                    << "[WARN] tile width units could not be derived; "
                       "using heuristic fallback\n";
            }
            if (geometry.usedHeightHeuristic)
            {
                std::cerr
                    << "[WARN] tile height units could not be derived; "
                       "using heuristic fallback\n";
            }
        }
        return geometry;
    }

    bool SVGConverter::emitTileImage(
        const std::vector<uint8_t> &encodedBytes,
        const std::string &mimeType,
        const TileGeometry &geometry,
        const char *imageRendering,
        const char *debugLabel)
    {
        const CGMPoint svgP = transform_.transformPoint(geometry.origin);
        const CGMPoint svgQ = transform_.transformPoint(addPoints(geometry.origin, geometry.pathVector));
        const CGMPoint svgR = transform_.transformPoint(addPoints(geometry.origin, geometry.lineVector));
        const CGMPoint svgDiag = transform_.transformPoint(
            addPoints(addPoints(geometry.origin, geometry.pathVector), geometry.lineVector));

        const double minX = std::min(std::min(svgP.x(), svgQ.x()), std::min(svgR.x(), svgDiag.x()));
        const double maxX = std::max(std::max(svgP.x(), svgQ.x()), std::max(svgR.x(), svgDiag.x()));
        const double minY = std::min(std::min(svgP.y(), svgQ.y()), std::min(svgR.y(), svgDiag.y()));
        const double maxY = std::max(std::max(svgP.y(), svgQ.y()), std::max(svgR.y(), svgDiag.y()));
        const double svgWidth = maxX - minX;
        const double svgHeight = maxY - minY;

        if (debug_fill_logging_)
        {
            std::cerr << "[svg] " << debugLabel
                      << " corners P(" << svgP.x() << "," << svgP.y()
                      << ") Q(" << svgQ.x() << "," << svgQ.y()
                      << ") R(" << svgR.x() << "," << svgR.y()
                      << ") Q+R(" << svgDiag.x() << "," << svgDiag.y() << ")"
                      << " emittedRect=(" << minX << "," << minY << ","
                      << svgWidth << "," << svgHeight << ")\n";
        }
        if (svgWidth <= 0.0 || svgHeight <= 0.0)
        {
            if (debug_fill_logging_)
            {
                std::cerr << "[svg] " << debugLabel << " skipped: degenerate rectangle\n";
            }
            return false;
        }

        const std::string base64Data = base64Encode(std::string(
            reinterpret_cast<const char *>(encodedBytes.data()),
            encodedBytes.size()));
        const std::string dataUri = "data:" + mimeType + ";base64," + base64Data;
        svg_output_ << "  <image x=\"" << minX << "\" y=\"" << minY
                    << "\" width=\"" << svgWidth << "\" height=\"" << svgHeight
                    << "\" style=\"image-rendering:" << imageRendering << "\" "
                    << clipPathAttribute();
        svg_output_ << "xlink:href=\"" << dataUri << "\" href=\"" << dataUri << "\" ";
        svg_output_ << "preserveAspectRatio=\"none\" />\n";
        return true;
    }

    void SVGConverter::advanceTileIndex()
    {
        if (in_tile_array_)
        {
            ++tile_current_index_;
        }
    }

    void SVGConverter::processBitonalTile(BitonalTile *cmd)
    {
        if (!cmd)
            return;

        const auto &imageData = cmd->imageData();
        if (imageData.empty())
        {
            advanceTileIndex();
            svg_output_ << "  <!-- Bitonal tile skipped: empty payload -->\n";
            return;
        }

        int compressionType = cmd->compressionType();
        if (!(compressionType == 2 || compressionType == 5 || compressionType == 6 || compressionType == 7 || compressionType == 9))
        {
            advanceTileIndex();
            svg_output_ << "  <!-- Bitonal tile compression " << compressionType << " unsupported for SVG export -->\n";
            return;
        }

        TileGeometry geometry = computeTileGeometry(*cmd, true);
        const int prefW = geometry.preferredWidth;
        const int prefH = geometry.preferredHeight;
        const int tileWidthCellCount = geometry.tileWidthCellCount;
        const int tileHeightCellCount = geometry.tileHeightCellCount;
        const int activeWidthCells = geometry.activeWidthCells;
        const int activeHeightCells = geometry.activeHeightCells;
        const int pixelWidth = geometry.pixelWidth;
        const int pixelHeight = geometry.pixelHeight;
        const double referenceWidth = geometry.referenceWidth;
        const double referenceHeight = geometry.referenceHeight;
        const double unitsPerCellPath = geometry.unitsPerCellPath;
        const double unitsPerCellLine = geometry.unitsPerCellLine;
        const double totalWidthUnits = geometry.totalWidthUnits;
        const double totalHeightUnits = geometry.totalHeightUnits;
        CGMPoint origin = geometry.origin;

        // Heuristic: when cell sizes are malformed (triggered fallback), the P corner position
        // may also be unreliable. Adjust position to better align with vector graphics.
        if (geometry.usedWidthFallback && compatibility_mode_ && in_tile_array_)
        {
            // Use 17% of VDC width as left margin, and shift Y up (decrease VDC Y by 7%)
            double adjustedX = referenceWidth * 0.30;
            double adjustedY = referenceHeight * 0.65;
            if (adjustedX >= 0.0)
            {
                origin = CGMPoint(adjustedX, adjustedY);
                if (debug_fill_logging_)
                {
                    std::cerr << "[svg] BitonalTile adjusted position from (" << tile_array_position_.x()
                              << "," << tile_array_position_.y() << ") to (" << adjustedX << "," << adjustedY
                              << ") (malformed CGM heuristic)\n";
                }
            }
        }

        const CGMPoint pathVector = geometry.pathVector;
        const CGMPoint lineVector = geometry.lineVector;
        geometry.origin = origin;

        if (debug_fill_logging_)
        {
            std::cerr << "[svg] BitonalTile geometry cells=(" << tileWidthCellCount << "," << tileHeightCellCount
                      << ") active=(" << activeWidthCells << "," << activeHeightCells << ") unitsPerCell=("
                      << unitsPerCellPath << "," << unitsPerCellLine << ") totals=("
                      << totalWidthUnits << "," << totalHeightUnits << ")\n";
            CGMPoint originSvg = transform_.transformPoint(origin);
            CGMPoint pathSvgEnd = transform_.transformPoint(addPoints(origin, pathVector));
            CGMPoint lineSvgEnd = transform_.transformPoint(addPoints(origin, lineVector));
            CGMPoint diagSvgEnd = transform_.transformPoint(addPoints(addPoints(origin, pathVector), lineVector));
            std::cerr << "[svg] BitonalTile corners P(" << originSvg.x() << "," << originSvg.y()
                      << ") Q(" << pathSvgEnd.x() << "," << pathSvgEnd.y()
                      << ") R(" << lineSvgEnd.x() << "," << lineSvgEnd.y()
                      << ") Q+R(" << diagSvgEnd.x() << "," << diagSvgEnd.y() << ")\n";
        }

        std::vector<uint8_t> encodedBytes;
        std::string mimeType;

        Color bgColor = resolveColor(cmd->backgroundColor(), ColorRole::Raster);
        Color fgColor = resolveColor(cmd->foregroundColor(), ColorRole::Raster);
        const bool applyTransparency = transparent_cell_active_ && transparent_cell_colour_enabled_;
        const Color transparentColor = transparent_cell_color_;
        if (debug_fill_logging_)
        {
            const Color &rawBg = cmd->backgroundColor().color();
            const Color &rawFg = cmd->foregroundColor().color();
            std::cerr << "[svg] BitonalTile colors background="
                      << static_cast<int>(bgColor.r) << ","
                      << static_cast<int>(bgColor.g) << ","
                      << static_cast<int>(bgColor.b)
                      << " foreground="
                      << static_cast<int>(fgColor.r) << ","
                      << static_cast<int>(fgColor.g) << ","
                      << static_cast<int>(fgColor.b)
                      << " rawBackground index=" << cmd->backgroundColor().colorIndex()
                      << " rawForeground index=" << cmd->foregroundColor().colorIndex()
                      << " rawBgRGB=("
                      << static_cast<int>(rawBg.r) << ","
                      << static_cast<int>(rawBg.g) << ","
                      << static_cast<int>(rawBg.b) << ") rawFgRGB=("
                      << static_cast<int>(rawFg.r) << ","
                      << static_cast<int>(rawFg.g) << ","
                      << static_cast<int>(rawFg.b) << ")\n";
            const std::string &sdr = cmd->dataRecord();
            std::cerr << "[svg] BitonalTile SDR bytes=" << sdr.size() << " prefix=";
            for (size_t i = 0; i < std::min<size_t>(sdr.size(), 16); ++i)
            {
                std::cerr << std::hex << std::uppercase
                          << std::setw(2) << std::setfill('0')
                          << (static_cast<int>(static_cast<unsigned char>(sdr[i])));
                if (i + 1 < std::min<size_t>(sdr.size(), 16))
                {
                    std::cerr << " ";
                }
            }
            std::cerr << std::dec << "\n";
        }

        bool logRaster = raster_logging_enabled_;
        RasterMetrics metrics;
        if (logRaster)
        {
            metrics.kind = RasterMetrics::Kind::BitonalTile;
            metrics.picture_index = current_picture_index_;
            metrics.command_index = current_command_index_;
            metrics.pixel_width = pixelWidth;
            metrics.pixel_height = pixelHeight;
            size_t totalPixels = static_cast<size_t>(std::max(pixelWidth, 0)) * static_cast<size_t>(std::max(pixelHeight, 0));
            metrics.transparent_pixels = 0;
            metrics.opaque_pixels = totalPixels;
            metrics.tcc_active = applyTransparency;
            metrics.had_tcc_match = false;
            metrics.origin = origin;
            metrics.path_vector = pathVector;
            metrics.line_vector = lineVector;
            metrics.mime_type.clear();
            metrics.compression = std::string("compression-") + std::to_string(compressionType);
        }

        if (compressionType == 2)
        {
            auto pngBytes = convertCcittToPng(imageData,
                                              static_cast<uint32_t>(pixelWidth),
                                              static_cast<uint32_t>(pixelHeight),
                                              bgColor,
                                              fgColor,
                                              applyTransparency,
                                              transparentColor,
                                              logRaster ? &metrics : nullptr);
            if (pngBytes)
            {
                encodedBytes = ensurePngHeader(*pngBytes,
                                               static_cast<uint32_t>(pixelWidth),
                                               static_cast<uint32_t>(pixelHeight));
                mimeType = "image/png";
            }
        }

        if (debug_fill_logging_)
        {
            std::cerr << "[svg] BitonalTile compression=" << compressionType
                      << " pref=" << prefW << "x" << prefH
                      << " pixels=" << pixelWidth << "x" << pixelHeight
                      << " transparent=" << (applyTransparency ? "yes" : "no")
                      << " mime=" << (mimeType.empty() ? "(pending)" : mimeType);
            if (!encodedBytes.empty())
            {
                std::cerr << " prefix=" << std::hex << static_cast<int>(encodedBytes[0] & 0xFF) << std::dec
                          << " units=(" << totalWidthUnits << "," << totalHeightUnits << ")";
            }
            std::cerr << "\n";
        }

        if (encodedBytes.empty())
        {
            if (compressionType == 5)
            {
                auto bmp = svg::TileRasterDecoder::buildBitonalBmp(
                    imageData,
                    pixelWidth,
                    pixelHeight,
                    bgColor,
                    fgColor);
                if (bmp)
                {
                    mimeType = "image/bmp";
                    encodedBytes = std::move(*bmp);
                }

#ifdef _WIN32
                // Re-encode BMP as PNG when WIC is available. Browsers handle
                // BMP data URIs, but headless rasterizers (resvg) do not, and
                // PNG is universally supported. Falls back to BMP if WIC
                // round-trip fails.
                if (auto png = convertBmpToPng(encodedBytes); png && !png->empty())
                {
                    encodedBytes = std::move(*png);
                    mimeType = "image/png";
                }
#endif
            }
            else
            {
                mimeType = deduceRasterMimeType(imageData, compressionType);
                encodedBytes = imageData;
                if (mimeType == "image/png")
                {
                    encodedBytes = ensurePngHeader(encodedBytes,
                                                   static_cast<uint32_t>(pixelWidth),
                                                   static_cast<uint32_t>(pixelHeight));
                }
            }
        }

        if (encodedBytes.empty())
        {
            advanceTileIndex();
            svg_output_ << "  <!-- Bitonal tile skipped: unable to decode payload -->\n";
            return;
        }

        if (debug_fill_logging_)
        {
            std::cerr << "[svg] Tile compression=" << compressionType
                      << " mime=" << mimeType;
            if (!encodedBytes.empty())
            {
                std::cerr << " prefix=" << std::hex << static_cast<int>(encodedBytes[0] & 0xFF)
                          << std::dec
                          << " units=(" << totalWidthUnits << "," << totalHeightUnits << ")";
            }
            std::cerr << "\n";
        }

        const char *imageRendering = (compressionType == 5 || compressionType == 2) ? "pixelated" : "auto";
        emitTileImage(encodedBytes, mimeType, geometry, imageRendering, "BitonalTile");
        advanceTileIndex();
    }

    void SVGConverter::processTile(Tile *cmd)
    {
        if (!cmd)
            return;

        const auto &imageData = cmd->imageData();
        if (imageData.empty())
        {
            advanceTileIndex();
            svg_output_ << "  <!-- Tile skipped: empty payload -->\n";
            return;
        }

        if (debug_fill_logging_ && !imageData.empty())
        {
            std::cerr << "[svg] Tile raw bytes:";
            size_t preview = std::min<size_t>(imageData.size(), 16);
            for (size_t i = 0; i < preview; ++i)
            {
                std::cerr << " " << std::hex << std::uppercase << static_cast<int>(imageData[i]);
            }
            std::cerr << std::dec << "\n";
        }

        int compressionType = cmd->compressionType();
        if (!(compressionType == 5 || compressionType == 6 || compressionType == 7 || compressionType == 9))
        {
            advanceTileIndex();
            svg_output_ << "  <!-- Tile compression " << compressionType << " unsupported for SVG export -->\n";
            return;
        }

        const TileGeometry geometry = computeTileGeometry(*cmd, false);
        const int prefW2 = geometry.preferredWidth;
        const int prefH2 = geometry.preferredHeight;
        const int tileWidthCellCount = geometry.tileWidthCellCount;
        const int tileHeightCellCount = geometry.tileHeightCellCount;
        const int activeWidthCells = geometry.activeWidthCells;
        const int activeHeightCells = geometry.activeHeightCells;
        const int pixelWidth = geometry.pixelWidth;
        const int pixelHeight = geometry.pixelHeight;
        const double unitsPerCellPath = geometry.unitsPerCellPath;
        const double unitsPerCellLine = geometry.unitsPerCellLine;
        const double totalWidthUnits = geometry.totalWidthUnits;
        const double totalHeightUnits = geometry.totalHeightUnits;
        if (debug_fill_logging_)
        {
            std::cerr << "[svg] Tile bitmapWidth=" << prefW2 << " bitmapHeight=" << prefH2
                      << " imageCellsPath=" << tile_image_cells_path_
                      << " imageCellsLine=" << tile_image_cells_line_ << "\n";
        }
        if (debug_fill_logging_)
        {
            std::cerr << "[svg] Tile pixelWidth=" << pixelWidth << " pixelHeight=" << pixelHeight << "\n";
        }

        if (debug_fill_logging_)
        {
            std::cerr << "[svg] Tile geometry cells=(" << tileWidthCellCount << "," << tileHeightCellCount
                      << ") active=(" << activeWidthCells << "," << activeHeightCells << ") unitsPerCell=("
                      << unitsPerCellPath << "," << unitsPerCellLine << ") totals=("
                      << totalWidthUnits << "," << totalHeightUnits << ")\n";
        }

        std::string mimeType;
        std::vector<uint8_t> encodedBytes;
#ifdef _WIN32
        const bool applyTransparency = transparent_cell_active_ && transparent_cell_colour_enabled_;
        const Color transparentColor = transparent_cell_color_;
#endif
        if (compressionType == 5)
        {
            svg::TileBitmapFormat bitmapFormat;
            bitmapFormat.width = std::max(
                1,
                prefW2 > 0
                    ? prefW2
                    : tile_cells_per_tile_path_);
            bitmapFormat.height = std::max(
                1,
                prefH2 > 0
                    ? prefH2
                    : tile_cells_per_tile_line_);
            bitmapFormat.selection_mode =
                cgm_file_->colorSelectionMode();
            bitmapFormat.color_model =
                cgm_file_->colorModel();
            bitmapFormat.local_color_precision =
                std::max(0, cmd->cellColorPrecision());
            bitmapFormat.direct_color_precision =
                cgm_file_->colourPrecision();
            bitmapFormat.color_index_precision =
                cgm_file_->colourIndexPrecision();

            auto bmp = svg::TileRasterDecoder::decodeBitmapToBmp(
                imageData,
                bitmapFormat,
                color_table_);
            if (bmp)
            {
                mimeType = "image/bmp";
                encodedBytes = std::move(*bmp);
            }

#ifdef _WIN32
            // Re-encode BMP as PNG for headless rasterizer compatibility.
            if (auto png = convertBmpToPng(encodedBytes); png && !png->empty())
            {
                encodedBytes = std::move(*png);
                mimeType = "image/png";
            }
#endif
        }
        else
        {
#ifdef _WIN32
            bool hasSignature = hasPngSignature(imageData);
            bool rebuiltFromChunks = false;
            if (compressionType == 9 && !hasSignature && imageData.size() >= 8 &&
                imageData[4] == 'I' && imageData[5] == 'D' && imageData[6] == 'A' && imageData[7] == 'T')
            {
                uint8_t derivedBitDepth = 0;
                uint8_t derivedColorType = 0;
                if (derivePngFormat(cgm_file_->colorSelectionMode(),
                                    cgm_file_->colorModel(),
                                    cmd->cellColorPrecision(),
                                    cgm_file_->colourPrecision(),
                                    cgm_file_->colourIndexPrecision(),
                                    derivedBitDepth,
                                    derivedColorType))
                {
                    auto rebuilt = buildPngFromIdat(imageData,
                                                    static_cast<uint32_t>(pixelWidth),
                                                    static_cast<uint32_t>(pixelHeight),
                                                    derivedBitDepth,
                                                    derivedColorType);
                    if (!rebuilt.empty())
                    {
                        mimeType = "image/png";
                        encodedBytes = std::move(rebuilt);
                        rebuiltFromChunks = true;
                    }
                }
            }

            if (!rebuiltFromChunks && (compressionType == 7 || compressionType == 9) && hasSignature)
            {
                if (applyTransparency)
                {
                    auto updated = reencodePngWithTransparency(imageData, applyTransparency, transparentColor, true);
                    if (updated)
                    {
                        mimeType = "image/png";
                        encodedBytes = std::move(*updated);
                        rebuiltFromChunks = true;
                    }
                }
                if (!rebuiltFromChunks)
                {
                    mimeType = "image/png";
                    encodedBytes = imageData;
                }
            }
#endif
            if (encodedBytes.empty())
            {
                mimeType = deduceRasterMimeType(imageData, compressionType);
                encodedBytes = imageData;
            }
        }

        if (encodedBytes.empty())
        {
            advanceTileIndex();
            svg_output_ << "  <!-- Tile skipped: unsupported payload -->\n";
            return;
        }

#ifdef _WIN32
        if (debug_fill_logging_ && compressionType == 9)
        {
            static int png_dump_index = 0;
            std::ostringstream dumpName;
            dumpName << "tile_dump_" << png_dump_index++ << ".png";
            std::ofstream dumpFile(dumpName.str(), std::ios::binary);
            if (dumpFile)
            {
                dumpFile.write(reinterpret_cast<const char *>(encodedBytes.data()), static_cast<std::streamsize>(encodedBytes.size()));
            }
        }
#endif

#ifdef _WIN32
        if (mimeType == "image/png")
        {
            bool encodedHasSignature = hasPngSignature(encodedBytes);
            if (!encodedHasSignature)
            {
                encodedBytes = ensurePngHeader(encodedBytes,
                                               static_cast<uint32_t>(pixelWidth),
                                               static_cast<uint32_t>(pixelHeight));
            }
            if (!applyTransparency && compressionType != 9)
            {
                if (auto opaque = ensureOpaquePng(encodedBytes, background_color_))
                {
                    encodedBytes = std::move(*opaque);
                }
            }
        }
#else
        if (mimeType == "image/png")
        {
            encodedBytes = ensurePngHeader(encodedBytes,
                                           static_cast<uint32_t>(pixelWidth),
                                           static_cast<uint32_t>(pixelHeight));
        }
#endif

        emitTileImage(encodedBytes, mimeType, geometry, "pixelated", "Tile");
        advanceTileIndex();
    }

    void SVGConverter::processGeneralizedDrawingPrimitive(GeneralizedDrawingPrimitive *cmd)
    {
        if (!cmd)
        {
            return;
        }

        ++gdp_placeholder_counter_;

        const auto &points = cmd->points();
        const std::string &dataRecord = cmd->dataRecord();

        svg_output_ << "  <g class=\"cgm-gdp\""
                    << " id=\"gdp-" << gdp_placeholder_counter_ << "\""
                    << " data-cgm-gdp-id=\"" << cmd->identifier() << "\""
                    << " data-cgm-gdp-point-count=\"" << points.size() << "\""
                    << " data-cgm-gdp-data-bytes=\"" << dataRecord.size() << "\">\n";

        std::ostringstream desc;
        desc << "GDP " << cmd->identifier() << " rendered via ISO/IEC 8632 fallback";

        if (points.size() >= 2)
        {
            std::ostringstream path;
            path.setf(std::ios::fixed);
            path << std::setprecision(3);

            CGMPoint first = transform_.transformPoint(points.front());
            path << "M " << first.x() << " " << first.y();

            for (size_t i = 1; i < points.size(); ++i)
            {
                CGMPoint svgPoint = transform_.transformPoint(points[i]);
                path << " L " << svgPoint.x() << " " << svgPoint.y();
            }

            svg_output_ << "    <path d=\"" << path.str() << "\" "
                        << current_style_.getStrokeStyle()
                        << "fill=\"none\" />\n";

            desc << " as a polyline through " << points.size() << " control point(s).";
        }
        else if (points.size() == 1)
        {
            double markerSize = current_style_.markerSize();
            double svgSize = transform_.transformLength(markerSize);
            if (svgSize <= 0.0)
            {
                svgSize = 1.0;
            }

            std::string markerPath = getMarkerPathData(current_style_.markerType(), svgSize);
            CGMPoint svgPoint = transform_.transformPoint(points.front());
            Color markerColor = current_style_.markerColor();

            char hex[8];
            std::snprintf(hex, sizeof(hex), "#%02X%02X%02X",
                          static_cast<int>(markerColor.r),
                          static_cast<int>(markerColor.g),
                          static_cast<int>(markerColor.b));

            svg_output_ << "    <path d=\"M " << svgPoint.x() << " " << svgPoint.y() << " "
                        << markerPath << "\" fill=\"" << hex << "\" stroke=\"" << hex
                        << "\" stroke-width=\"" << (svgSize * 0.1) << "\" />\n";

            desc << " as a point marker fallback.";
        }
        else
        {
            desc << "; no geometry emitted because no control points were supplied.";
        }

        if (!dataRecord.empty())
        {
            desc << " SDR length: " << dataRecord.size() << " byte(s).";
        }

        svg_output_ << "    <desc>" << escapeXmlText(desc.str()) << "</desc>\n";
        svg_output_ << "  </g>\n";
    }

    void SVGConverter::processPolySymbol(PolySymbol *cmd)
    {
        if (!cmd)
        {
            return;
        }

        ++symbol_placeholder_counter_;

        const auto &points = cmd->points();
        int symbolIndex = cmd->index();
        std::vector<CGMPoint> anchors = points;
        bool usedDefaultAnchor = false;
        if (anchors.empty())
        {
            anchors.emplace_back(0.0, 0.0);
            usedDefaultAnchor = true;
        }

        std::string resolvedName;
        if (!symbol_libraries_.empty())
        {
            if (symbolIndex >= 0 && symbolIndex < static_cast<int>(symbol_libraries_.size()))
            {
                resolvedName = symbol_libraries_[symbolIndex];
            }
            else if (symbolIndex > 0 && symbolIndex - 1 < static_cast<int>(symbol_libraries_.size()))
            {
                resolvedName = symbol_libraries_[symbolIndex - 1];
            }
        }

        bool resolved = !resolvedName.empty();

        std::string defId;
        std::string symbolSource;
        std::string symbolFragment;

        if (resolved)
        {
            defId = ensureSymbolDefinition(resolvedName);
            auto sourceIt = symbol_definition_sources_.find(resolvedName);
            if (sourceIt != symbol_definition_sources_.end() && !sourceIt->second.empty())
            {
                symbolSource = sourceIt->second;
            }
            auto fragmentIt = symbol_definition_fragments_.find(resolvedName);
            if (fragmentIt != symbol_definition_fragments_.end() && !fragmentIt->second.empty())
            {
                symbolFragment = fragmentIt->second;
            }
        }

        bool definitionReady = resolved && !defId.empty();

        svg_output_ << "  <g class=\"cgm-symbol\""
                    << " id=\"symbol-" << symbol_placeholder_counter_ << "\""
                    << " data-cgm-symbol-index=\"" << symbolIndex << "\""
                    << " data-cgm-point-count=\"" << points.size() << "\""
                    << " data-cgm-symbol-resolved=\"" << (definitionReady ? "true" : "false") << "\"";

        if (resolved)
        {
            svg_output_ << " data-cgm-symbol-name=\"" << escapeXmlAttribute(resolvedName) << "\"";
        }
        else
        {
            svg_output_ << " data-cgm-symbol-name=\"\"";
        }
        if (!symbolSource.empty())
        {
            svg_output_ << " data-cgm-symbol-source=\"" << escapeXmlAttribute(symbolSource) << "\"";
        }
        if (!symbolFragment.empty())
        {
            svg_output_ << " data-cgm-symbol-fragment=\"" << escapeXmlAttribute(symbolFragment) << "\"";
        }

        svg_output_ << ">\n";

        std::ostringstream desc;
        if (definitionReady)
        {
            desc << "Symbol '" << resolvedName << "' placed at " << anchors.size() << " anchor(s).";
            if (usedDefaultAnchor)
            {
                desc << " Default origin used because no anchor points were supplied.";
            }
        }
        else
        {
            desc << "Symbol index " << symbolIndex
                 << " unresolved; marker fallback applied per ISO/IEC 8632-1 element 27.";
            if (usedDefaultAnchor)
            {
                desc << " Default origin used because no anchor points were supplied.";
            }
        }

        if (definitionReady)
        {
            for (const auto &anchor : anchors)
            {
                CGMPoint anchorSvg = transform_.transformPoint(anchor);
                svg_output_ << "    <use xlink:href=\"#" << defId << "\" x=\"" << anchorSvg.x()
                            << "\" y=\"" << anchorSvg.y() << "\" />\n";
            }
        }
        else
        {
            double markerSize = current_style_.markerSize();
            double svgSize = transform_.transformLength(markerSize);
            if (svgSize <= 0.0)
            {
                svgSize = 1.0;
            }

            std::string markerPath = getMarkerPathData(current_style_.markerType(), svgSize);
            Color markerColor = current_style_.markerColor();
            char hex[8];
            std::snprintf(hex, sizeof(hex), "#%02X%02X%02X",
                          static_cast<int>(markerColor.r),
                          static_cast<int>(markerColor.g),
                          static_cast<int>(markerColor.b));
            double strokeWidth = svgSize * 0.1;

            if (markerPath.empty())
            {
                for (const auto &anchor : anchors)
                {
                    CGMPoint anchorSvg = transform_.transformPoint(anchor);
                    svg_output_ << "    <circle cx=\"" << anchorSvg.x() << "\" cy=\"" << anchorSvg.y()
                                << "\" r=\"" << (svgSize * 0.5) << "\" fill=\"" << hex
                                << "\" stroke=\"" << hex << "\" stroke-width=\"" << strokeWidth << "\" />\n";
                }
            }
            else
            {
                for (const auto &anchor : anchors)
                {
                    CGMPoint anchorSvg = transform_.transformPoint(anchor);
                    svg_output_ << "    <path d=\"M " << anchorSvg.x() << " " << anchorSvg.y()
                                << " " << markerPath << "\" fill=\"" << hex << "\" stroke=\"" << hex
                                << "\" stroke-width=\"" << strokeWidth << "\" />\n";
                }
            }
        }

        std::string title = definitionReady ? "CGM PolySymbol (" + resolvedName + ")" : "CGM PolySymbol fallback";

        svg_output_ << "    <title>" << escapeXmlText(title) << "</title>\n";
        svg_output_ << "    <desc>" << escapeXmlText(desc.str()) << "</desc>\n";
        svg_output_ << "  </g>\n";
    }

    double SVGConverter::hatchStrokeWidth() const
    {
        double strokeWidth = current_style_.lineWidth();
        double hairline = nominal_line_width_svg_ / 256.0;
        if (!std::isfinite(hairline) || hairline <= 0.0)
        {
            hairline = 0.01;
        }
        if (!std::isfinite(strokeWidth) || strokeWidth <= 0.0)
        {
            strokeWidth = hairline;
        }
        else if (strokeWidth < hairline)
        {
            strokeWidth = hairline;
        }
        return strokeWidth;
    }

    std::string SVGConverter::ensureParallelHatchPattern(int hatchIndex, const HatchDefinition &definition, const Color &color)
    {
        char hex[8];
        std::snprintf(hex, sizeof(hex), "#%02X%02X%02X", (int)color.r, (int)color.g, (int)color.b);

        std::ostringstream keyStream;
        keyStream.setf(std::ios::fixed);
        keyStream << std::setprecision(4)
                  << "def-" << hatchIndex << "-" << hex
                  << ":" << definition.styleIndicator
                  << ":" << definition.direction.x() << "," << definition.direction.y()
                  << ":" << definition.spacing.x() << "," << definition.spacing.y();
        std::string key = keyStream.str();

        auto cached = hatch_pattern_ids_.find(key);
        if (cached != hatch_pattern_ids_.end())
        {
            return cached->second;
        }

        auto vectorFromDefinition =
            [&](const CGMPoint &vec) -> svg::PatternVector
        {
            CGMPoint origin(0.0, 0.0);
            CGMPoint dest(vec.x(), vec.y());
            CGMPoint originSvg = transform_.transformPoint(origin);
            CGMPoint destSvg = transform_.transformPoint(dest);
            return {
                destSvg.x() - originSvg.x(),
                destSvg.y() - originSvg.y()};
        };

        svg::ParallelHatchInput geometryInput;
        geometryInput.direction_vector =
            vectorFromDefinition(definition.direction);
        geometryInput.spacing_vector =
            vectorFromDefinition(definition.spacing);
        geometryInput.stroke_width = hatchStrokeWidth();
        if (has_fill_reference_point_)
        {
            const CGMPoint referenceSvg =
                transform_.transformPoint(fill_reference_point_);
            geometryInput.reference_point = {
                referenceSvg.x(),
                referenceSvg.y()};
            geometryInput.has_reference_point = true;
        }

        const auto geometry =
            svg::PatternGeometryResolver::resolveParallelHatch(
                geometryInput);
        if (!geometry.valid())
        {
            if (compatibility_mode_)
            {
                return {};
            }

            switch (geometry.issue)
            {
            case svg::HatchGeometryIssue::DegenerateDefinition:
                throw std::runtime_error("Hatch definition vectors are degenerate; rerun with --profile=compat for fallback rendering");
            case svg::HatchGeometryIssue::ParallelSpacing:
                throw std::runtime_error("Hatch definition spacing vector is parallel to hatch direction; rerun with --profile=compat for fallback rendering");
            case svg::HatchGeometryIssue::InvalidDirection:
                throw std::runtime_error("Hatch direction produced invalid transform vectors; rerun with --profile=compat for fallback rendering");
            case svg::HatchGeometryIssue::DegenerateBasis:
            case svg::HatchGeometryIssue::None:
                break;
            }
        }

        if (!defs_open_)
        {
            svg_output_ << "  <defs>\n";
            defs_open_ = true;
        }

        std::string patternId = "cgm-hatch-" + std::to_string(++pattern_counter_);
        svg_output_ << "    <pattern id=\"" << patternId
                    << "\" patternUnits=\"userSpaceOnUse\" patternContentUnits=\"userSpaceOnUse\" width=\"1\" height=\"1\"";
        svg_output_ << " patternTransform=\"matrix("
                    << geometry.first_basis.x << " "
                    << geometry.first_basis.y << " "
                    << geometry.second_basis.x << " "
                    << geometry.second_basis.y << " "
                    << geometry.phase.x << " "
                    << geometry.phase.y << ")\">\n";

        svg_output_ << "      <rect x=\"0\" y=\"0\" width=\"1\" height=\"1\" fill=\"none\"/>\n";

        for (const auto &line : geometry.lines)
        {
            svg_output_ << "      <line x1=\"" << line.x1
                        << "\" y1=\"" << line.y1
                        << "\" x2=\"" << line.x2
                        << "\" y2=\"" << line.y2
                        << "\" stroke=\"" << hex
                        << "\" stroke-width=\""
                        << geometry.stroke_width << "\"";
            if (geometry.square_line_caps)
            {
                svg_output_ << " stroke-linecap=\"square\"";
            }
            svg_output_ << " />\n";
        }
        svg_output_ << "    </pattern>\n";

        hatch_pattern_ids_[key] = patternId;
        return patternId;
    }

    std::string SVGConverter::ensureHatchPattern(int hatchIndex, const Color &color)
    {
        auto defIt = hatch_definitions_.find(hatchIndex);
        if (defIt != hatch_definitions_.end())
        {
            if (defIt->second.styleIndicator == 0)
            {
                std::string customId = ensureParallelHatchPattern(hatchIndex, defIt->second, color);
                if (!customId.empty())
                {
                    return customId;
                }
            }
            else
            {
                if (!compatibility_mode_)
                {
                    std::ostringstream message;
                    message << "HATCH STYLE DEFINITION styleIndicator=" << defIt->second.styleIndicator
                            << " is not supported in strict profile; rerun with --profile=compat for fallback rendering";
                    throw std::runtime_error(message.str());
                }
#ifndef NDEBUG
                if (std::getenv("SVG_DEBUG_HATCH"))
                {
                    std::cerr << "[hatch] fallback to default pattern for styleIndicator="
                              << defIt->second.styleIndicator << " index=" << hatchIndex << "\n";
                }
#endif
            }
        }

        // Key by index + color
        char hex[8];
        std::snprintf(hex, sizeof(hex), "#%02X%02X%02X", (int)color.r, (int)color.g, (int)color.b);
        std::string key = std::to_string(hatchIndex) + std::string("-") + hex;
        auto it = hatch_pattern_ids_.find(key);
        if (it != hatch_pattern_ids_.end())
            return it->second;

        auto userVectorFromVdc =
            [&](double vx, double vy) -> svg::PatternVector
        {
            CGMPoint origin(0.0, 0.0);
            CGMPoint endpoint(vx, vy);
            CGMPoint originSvg = transform_.transformPoint(origin);
            CGMPoint endSvg = transform_.transformPoint(endpoint);
            return {
                endSvg.x() - originSvg.x(),
                endSvg.y() - originSvg.y()};
        };

        svg::StandardHatchInput geometryInput;
        geometryInput.hatch_index = hatchIndex;
        geometryInput.x_basis =
            userVectorFromVdc(1.0, 0.0);
        geometryInput.y_basis =
            userVectorFromVdc(0.0, 1.0);
        geometryInput.viewbox_width = svg_viewbox_width_;
        geometryInput.viewbox_height = svg_viewbox_height_;
        geometryInput.compatibility_mode = compatibility_mode_;
        if (has_fill_reference_point_)
        {
            const CGMPoint referenceSvg =
                transform_.transformPoint(fill_reference_point_);
            geometryInput.reference_point = {
                referenceSvg.x(),
                referenceSvg.y()};
            geometryInput.has_reference_point = true;
        }

        const auto geometry =
            svg::PatternGeometryResolver::resolveStandardHatch(
                geometryInput);
        if (geometry.issue ==
            svg::HatchGeometryIssue::DegenerateBasis)
        {
            throw std::runtime_error("HATCH fallback basis is degenerate; rerun with --profile=compat for heuristic rendering");
        }

        if (!defs_open_)
        {
            svg_output_ << "  <defs>\n";
            defs_open_ = true;
        }

        std::string patId = "cgm-hatch-" + std::to_string(++pattern_counter_);
        svg_output_ << "    <pattern id=\"" << patId
                    << "\" patternUnits=\"userSpaceOnUse\" patternContentUnits=\"userSpaceOnUse\" width=\""
                    << geometry.pattern_width << "\" height=\""
                    << geometry.pattern_height << "\""
                    << " patternTransform=\"matrix("
                    << geometry.first_basis.x << " "
                    << geometry.first_basis.y << " "
                    << geometry.second_basis.x << " "
                    << geometry.second_basis.y << " "
                    << geometry.phase.x << " "
                    << geometry.phase.y << ")\">\n";
        // background transparent; draw hatch lines using stroke color
        svg_output_ << "      <rect x=\"0\" y=\"0\" width=\""
                    << geometry.pattern_width << "\" height=\""
                    << geometry.pattern_height
                    << "\" fill=\"none\"/>\n";
        auto strokeHex = [&]() {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", (int)color.r, (int)color.g, (int)color.b);
            return std::string(buf);
        }();

        for (const auto &line : geometry.lines)
        {
            svg_output_ << "      <line x1=\"" << line.x1
                        << "\" y1=\"" << line.y1
                        << "\" x2=\"" << line.x2
                        << "\" y2=\"" << line.y2
                        << "\" stroke=\"" << strokeHex
                        << "\" stroke-width=\""
                        << geometry.stroke_width << "\" />\n";
        }
        svg_output_ << "    </pattern>\n";
        hatch_pattern_ids_[key] = patId;
        return patId;
    }

    void SVGConverter::applyFillBundle(int index)
    {
        active_fill_bundle_index_ = std::max(index, 1);
        auto it = fill_bundles_.find(active_fill_bundle_index_);
        if (it != fill_bundles_.end())
        {
            applyFillBundleEntry(it->second, "fill-bundle");
        }
        else if (debug_fill_logging_)
        {
            std::cerr << "[svg] fill bundle index=" << active_fill_bundle_index_
                      << " has no representation\n";
        }
    }

    void SVGConverter::applyFillBundleEntry(const FillBundleEntry &entry, const char *reason)
    {
        int style = entry.interiorStyle;
        if (style < 0)
        {
            style = 0;
        }
        current_style_.setFillStyle(style);
        current_style_.setHatchIndex(std::max(entry.hatchIndex, 1));
        current_style_.setPatternIndex(std::max(entry.patternIndex, 1));

        std::string label = "FILL BUNDLE";
        if (reason && reason[0] != '\0')
        {
            label.append(" (");
            label.append(reason);
            label.push_back(')');
        }

        Color resolved = resolveColor(entry.color, ColorRole::Fill, label.c_str());
        current_style_.setFillColor(resolved);

        if (debug_fill_logging_)
        {
            std::cerr << "[svg] applied fill bundle style=" << interiorStyleName(style)
                      << " color=" << static_cast<int>(resolved.r) << ","
                      << static_cast<int>(resolved.g) << ","
                      << static_cast<int>(resolved.b);
            if (entry.color.isIndexed())
            {
                std::cerr << " (index=" << entry.color.colorIndex() << ")";
            }
            if (reason && reason[0] != '\0')
            {
                std::cerr << " source=" << reason;
            }
            std::cerr << "\n";
        }
    }

    // Known blind spot: seven emitters take SVGStyle::getStyleWithEdges() instead of coming
    // through here, and that calls getFillStyle() directly - so they resolve neither patterns
    // nor hatches, and the warnings below never fire for them. processCircularArc3PointClose
    // is one, which is why the pattern fill in samples/ata30-cgms/crar3c01.cgm is lost with
    // nothing said. Tracked separately; routing those sites through here is a behaviour change
    // that needs its own visual-regression pass, not a rider on a diagnostic commit.
    std::string SVGConverter::getFillAttributeForCurrentStyle()
    {
        int fs = current_style_.fillStyle();
        switch (fs)
        {
        case 0: // HOLLOW
        case 4: // EMPTY
            return "fill=\"none\" ";
        case 2: // PATTERN
        {
            std::string patId = ensurePatternFill(current_style_.patternIndex());
            if (!patId.empty())
            {
                return std::string("fill=\"url(#") + patId + ")\" ";
            }
            // Resolution failed and we are about to paint a flat colour where a tiled
            // pattern belongs. Indistinguishable in the output from a deliberate solid
            // fill, so it has to be said out loud.
            if (!pattern_fallback_warning_emitted_)
            {
                std::cerr << "[svg] PATTERN INDEX " << current_style_.patternIndex()
                          << " did not resolve to a pattern; filling solid instead\n";
                pattern_fallback_warning_emitted_ = true;
            }
            break;
        }
        case 3: // HATCH
        {
            std::string patId = ensureHatchPattern(current_style_.hatchIndex(), current_style_.fillColor());
            if (!patId.empty())
            {
                return std::string("fill=\"url(#") + patId + ")\" ";
            }
            if (!hatch_fallback_warning_emitted_)
            {
                std::cerr << "[svg] HATCH INDEX " << current_style_.hatchIndex()
                          << " did not resolve to a pattern; filling solid instead\n";
                hatch_fallback_warning_emitted_ = true;
            }
            break;
        }
        default:
            // SOLID (1) also lands here and is genuinely handled by getFillStyle() - it is
            // not a fallback and must not warn. Everything else is: 5 (geometric pattern),
            // 6 (interpolated), or a value outside the standard range. Those fall through
            // and get painted in the current fill colour, which is a plausible looking
            // result rather than an obviously broken one - precisely why it needs saying.
            // interpolated-interior-01 renders as a solid black square where a gradient
            // belongs, and scores worse than any other test in the conformance screen.
            if (fs != 1 && unsupported_fill_styles_warned_.insert(fs).second)
            {
                std::cerr << "[svg] INTERIOR STYLE " << fs << " (" << interiorStyleName(fs)
                          << ") is not rendered; filling solid instead\n";
            }
            break;
        }
        return current_style_.getFillStyle();
    }

    std::string SVGConverter::buildFillAndEdgeAttributes(bool overrideEdgeVisibility, bool edgeVisibleOverride)
    {
        std::ostringstream buffer;
        buffer << getFillAttributeForCurrentStyle();

        bool edgeVisible = overrideEdgeVisibility ? edgeVisibleOverride : current_style_.edgeVisibility();
        // ISO 8632-1 §6.5.18 INTERIOR STYLE = HOLLOW: closed-figure primitives
        // are defined by their outline — the perimeter is always drawn,
        // independent of EDGE VISIBILITY. SVGStyle::getEdgeStyle() already
        // honours this rule; bypass the short-circuit here so it's reachable.
        bool hollowForcesEdge = (current_style_.fillStyle() == 0);
        if (edgeVisible || hollowForcesEdge)
        {
            buffer << current_style_.getEdgeStyle();
        }
        else
        {
            buffer << "stroke=\"none\" ";
        }
        return buffer.str();
    }

    const char *SVGConverter::interiorStyleName(int style)
    {
        switch (style)
        {
        case 0:
            return "HOLLOW";
        case 1:
            return "SOLID";
        case 2:
            return "PATTERN";
        case 3:
            return "HATCH";
        case 4:
            return "EMPTY";
        case 5:
            return "GEOMETRIC PATTERN";
        case 6:
            return "INTERPOLATED";
        default:
            return "UNKNOWN";
        }
    }

    void SVGConverter::resetTileArrayState()
    {
        in_tile_array_ = false;
        tile_array_position_ = CGMPoint(0.0, 0.0);
        tile_path_direction_ = 0;
        tile_line_direction_ = 90;
        tile_cell_width_ = 1.0;
        tile_cell_height_ = 1.0;
        tile_tiles_in_path_ = 1;
        tile_tiles_in_line_ = 1;
        tile_cells_per_tile_path_ = 1;
        tile_cells_per_tile_line_ = 1;
        tile_image_offset_path_ = 0;
        tile_image_offset_line_ = 0;
        tile_image_cells_path_ = 0;
        tile_image_cells_line_ = 0;
        tile_current_index_ = 0;
    }

    void SVGConverter::resetPatternState()
    {
        hatch_pattern_ids_.clear();
        pattern_fill_ids_.clear();
        pattern_tables_.clear();
        pattern_sizes_.clear();
        active_pattern_size_ = PatternSizeData{};
        has_active_pattern_size_ = false;
        hatch_definitions_.clear();
        fill_reference_point_ = CGMPoint(0.0, 0.0);
        has_fill_reference_point_ = false;
        last_defined_pattern_index_ = 1;
    }

    void SVGConverter::resetPictureState()
    {
        current_style_ = SVGStyle();
        fill_bundles_.clear();
        active_fill_bundle_index_ = 1;
        has_layer_aps_ = false;
        has_linkuri_aps_ = false;
        has_viewcontext_aps_ = false;

        // Reset two-pots colour slots to ISO 8632-1 §A.1 defaults
        // (foreground/colour-index-1 = black for all attributes).
        line_color_slots_ = ColorSlots(Color::Black(), Color::Black());
        fill_color_slots_ = ColorSlots(Color::Black(), Color::Black());
        edge_color_slots_ = ColorSlots(Color::Black(), Color::Black());
        text_color_slots_ = ColorSlots(Color::Black(), Color::Black());
        active_color_mode_ = cgm_file_ ? cgm_file_->colorSelectionMode()
                                        : ColorSelectionMode::INDEXED;

        character_orientation_base_ = CGMPoint(1.0, 0.0);
        character_orientation_up_ = CGMPoint(0.0, 1.0);

        pending_text_active_ = false;
        pending_text_position_ = CGMPoint(0.0, 0.0);
        pending_text_segments_.clear();
        pending_text_h_align_ = current_style_.textHAlign();
        pending_text_v_align_ = current_style_.textVAlign();
        pending_text_rotation_deg_ = 0.0;
        pending_text_has_rotation_ = false;
        pending_is_restricted_text_ = false;
        pending_restricted_delta_width_ = 0.0;
        pending_restricted_delta_height_ = 0.0;
        has_last_text_position_ = false;
        last_text_position_ = CGMPoint(0.0, 0.0);

        clip_rectangle_defined_ = false;
        clip_rect_first_ = CGMPoint(0.0, 0.0);
        clip_rect_second_ = CGMPoint(0.0, 0.0);
        clip_enabled_ = false;
        clip_path_attribute_.clear();
        clip_path_cache_.clear();

        in_protection_region_ = false;
        protection_region_indicator_ = 1;
        active_protection_region_index_ = -1;
        protection_region_paths_.clear();
        protection_region_definitions_.clear();
        protection_region_clip_ids_.clear();

        transparent_cell_active_ = false;
        transparent_cell_color_ = Color::White();

        resetTileArrayState();
        resetPatternState();

        color_table_.clear();
        initializeDefaultColorTable();

        current_picture_name_.clear();
    }

    std::string SVGConverter::ensurePatternFill(int patternIndex)
    {
        auto tableIt = pattern_tables_.find(patternIndex);
        if (tableIt == pattern_tables_.end())
        {
            if (!compatibility_mode_)
            {
                throw std::runtime_error("Pattern index has no PATTERN TABLE; rerun with --profile=compat for tolerant decoding");
            }
            return {};
        }

        auto cached = pattern_fill_ids_.find(patternIndex);
        if (cached != pattern_fill_ids_.end())
        {
            return cached->second;
        }

        const auto &table = tableIt->second;
        // Resolve PATTERN SIZE for this index. Per-index bindings come from
        // either an explicit PATTERN SIZE element keyed by last_defined index
        // or auto-inheritance from the global active size at table-definition
        // time (see processPatternTable). Tables that ended up unbound use a
        // viewbox-derived default so the tile is visible at typical raster
        // scales (1 VDC unit would be sub-pixel).
        PatternSizeData sizeData;
        auto sizeIt = pattern_sizes_.find(patternIndex);
        if (sizeIt != pattern_sizes_.end())
        {
            sizeData = sizeIt->second;
        }
        else
        {
            double w = svg_viewbox_width_ > 0.0 ? svg_viewbox_width_ : 1.0;
            double h = svg_viewbox_height_ > 0.0 ? svg_viewbox_height_ : 1.0;
            double tile = std::min(w, h) / 16.0;
            if (tile <= 0.0) tile = 1.0;
            sizeData.widthX = tile;
            sizeData.widthY = 0.0;
            sizeData.heightX = 0.0;
            sizeData.heightY = tile;
        }

        auto vectorFromVdc = [&](double vx, double vy) {
            CGMPoint origin(0.0, 0.0);
            CGMPoint endpoint(vx, vy);
            CGMPoint originSvg = transform_.transformPoint(origin);
            CGMPoint endSvg = transform_.transformPoint(endpoint);
            return svg::PatternVector{
                endSvg.x() - originSvg.x(),
                endSvg.y() - originSvg.y()};
        };

        svg::PatternGeometryInput geometryInput;
        geometryInput.columns = table.nx;
        geometryInput.rows = table.ny;
        geometryInput.cell_count = table.cells.size();
        geometryInput.width_vector =
            vectorFromVdc(sizeData.widthX, sizeData.widthY);
        geometryInput.height_vector =
            vectorFromVdc(sizeData.heightX, sizeData.heightY);
        geometryInput.fallback_x_vector =
            vectorFromVdc(1.0, 0.0);
        geometryInput.fallback_y_vector =
            vectorFromVdc(0.0, 1.0);
        geometryInput.compatibility_mode = compatibility_mode_;
        if (has_fill_reference_point_)
        {
            const CGMPoint referenceSvg =
                transform_.transformPoint(fill_reference_point_);
            geometryInput.reference_point = {
                referenceSvg.x(),
                referenceSvg.y()};
            geometryInput.has_reference_point = true;
        }

        const auto geometry =
            svg::PatternGeometryResolver::resolve(geometryInput);
        switch (geometry.issue)
        {
        case svg::PatternGeometryIssue::InvalidGrid:
            throw std::runtime_error("PATTERN TABLE grid is invalid (nx/ny <= 0); rerun with --profile=compat for heuristic fixes");
        case svg::PatternGeometryIssue::CellCountMismatch:
        {
            std::ostringstream message;
            message << "PATTERN TABLE cell count (" << table.cells.size()
                    << ") does not match nx*ny (" << geometry.expected_cells
                    << "); rerun with --profile=compat for tolerant decoding";
            throw std::runtime_error(message.str());
        }
        case svg::PatternGeometryIssue::DegenerateSize:
            throw std::runtime_error("PATTERN SIZE vectors are degenerate; rerun with --profile=compat for heuristic fallbacks");
        case svg::PatternGeometryIssue::ZeroTileStep:
            throw std::runtime_error("PATTERN SIZE produced zero tile vectors; rerun with --profile=compat for heuristic fallbacks");
        case svg::PatternGeometryIssue::None:
            break;
        }

        if (!defs_open_)
        {
            svg_output_ << "  <defs>\n";
            defs_open_ = true;
        }

        std::string patternId = "cgm-pattern-" + std::to_string(++pattern_counter_);
        svg_output_ << "    <pattern id=\"" << patternId
                    << "\" patternUnits=\"userSpaceOnUse\" patternContentUnits=\"userSpaceOnUse\""
                    << " width=\"" << geometry.columns
                    << "\" height=\"" << geometry.rows << "\"";

        svg_output_ << " patternTransform=\"matrix("
                    << geometry.width_step.x << " "
                    << geometry.width_step.y << " "
                    << geometry.height_step.x << " "
                    << geometry.height_step.y << " "
                    << geometry.phase.x << " "
                    << geometry.phase.y << ")\"";
        svg_output_ << ">\n";

        auto colorToHexString = [](const Color &color) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "#%02X%02X%02X",
                          static_cast<int>(color.r),
                          static_cast<int>(color.g),
                          static_cast<int>(color.b));
            return std::string(buf);
        };

        for (int row = 0; row < geometry.rows; ++row)
        {
            for (int col = 0; col < geometry.columns; ++col)
            {
                size_t idx =
                    static_cast<size_t>(row) *
                        static_cast<size_t>(geometry.columns) +
                    static_cast<size_t>(col);
                if (idx >= table.cells.size())
                {
                    if (!compatibility_mode_)
                    {
                        throw std::runtime_error("PATTERN TABLE cell count underrun; rerun with --profile=compat for tolerant decoding");
                    }
                    continue;
                }
                double x = static_cast<double>(col);
                // CGMOpen/NIST WebCGM 1.0 reference renderers place pattern
                // data row 0 at the top of the rendered tile (PATTBL02 "7"
                // glyph, INTSTL05 bowtie). Emit at y = ny-1-row so the
                // negative-Y patternTransform lands row 0 at the visual top.
                double y =
                    static_cast<double>(
                        geometry.rows - 1 - row);
                svg_output_ << "      <rect x=\"" << x << "\" y=\"" << y << "\" width=\"1\" height=\"1\" fill=\""
                            << colorToHexString(table.cells[idx]) << "\" />\n";
            }
        }

        svg_output_ << "    </pattern>\n";

        pattern_fill_ids_[patternIndex] = patternId;
        return patternId;
    }

    std::string SVGConverter::resolveFontFamilyFromIndex(int index) const
    {
        if (index <= 0)
        {
            index = 1;
        }

        std::string rawName;
        if (index > 0 && static_cast<size_t>(index) <= font_list_.size())
        {
            rawName = font_list_[static_cast<size_t>(index - 1)];
        }

        if (rawName.empty())
        {
            // Use configured fallback stack if provided
            if (!text_options_.font_fallback_stack.empty())
            {
                return formatFontStack(text_options_.font_fallback_stack);
            }
            return formatFontStack({"Arial", "Helvetica", "sans-serif"});
        }

        // Apply substitutions/overrides first; if found, return directly
        std::string mapped = resolveFontFamilyNameFromRaw(rawName);
        if (!mapped.empty())
        {
            return mapped;
        }

        auto stack = buildFontStack(rawName);
        return formatFontStack(stack);
    }

    void SVGConverter::rebuildCompiledFontSubstitutions() const
    {
        compiled_font_substitutions_.clear();
        compiled_font_substitutions_.reserve(text_options_.font_substitutions.size());

        for (const auto &sub : text_options_.font_substitutions)
        {
            try
            {
                compiled_font_substitutions_.emplace_back(std::regex(sub.first, std::regex::icase), sub.second);
            }
            catch (...)
            {
                // Ignore invalid regex; thin shim should not throw.
            }
        }

        font_substitutions_cache_dirty_ = false;
    }

    std::string SVGConverter::resolveFontFamilyNameFromRaw(const std::string &raw) const
    {
        std::string candidate = raw;

        // Compile regex substitutions once per options change to avoid per-text recompilation.
        if (font_substitutions_cache_dirty_)
        {
            rebuildCompiledFontSubstitutions();
        }

        // Apply regex substitutions to canonicalize name.
        for (const auto &sub : compiled_font_substitutions_)
        {
            if (std::regex_search(candidate, sub.first))
            {
                candidate = sub.second;
                break;
            }
        }

        // Direct override to CSS stack
        auto it = text_options_.font_overrides.find(candidate);
        if (it != text_options_.font_overrides.end())
        {
            return it->second;
        }
        return {};
    }

    std::string SVGConverter::primaryFontFromStack(const std::string &stack) const
    {
        std::string trimmed = opencgm::utils::trimString(stack);
        if (trimmed.empty())
        {
            return {};
        }

        size_t commaPos = trimmed.find(',');
        std::string primary = opencgm::utils::trimString(trimmed.substr(0, commaPos));
        if (primary.size() >= 2 &&
            ((primary.front() == '"' && primary.back() == '"') ||
             (primary.front() == '\'' && primary.back() == '\'')))
        {
            primary = primary.substr(1, primary.size() - 2);
        }
        return primary;
    }

    double SVGConverter::baselineAdjustmentForFont(const std::string &fontFamily) const
    {
        if (text_options_.font_baseline_adjustments.empty() && std::fabs(text_options_.default_baseline_adjustment) < 1e-9)
        {
            return 0.0;
        }

        std::string primary = primaryFontFromStack(fontFamily);
        if (primary.empty())
        {
            return text_options_.default_baseline_adjustment;
        }

        auto direct = text_options_.font_baseline_adjustments.find(primary);
        if (direct != text_options_.font_baseline_adjustments.end())
        {
            return direct->second;
        }

        std::string lowerPrimary = primary;
        std::transform(lowerPrimary.begin(), lowerPrimary.end(), lowerPrimary.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });

        for (const auto &entry : text_options_.font_baseline_adjustments)
        {
            std::string keyLower = entry.first;
            std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            if (keyLower == lowerPrimary)
            {
                return entry.second;
            }
        }

        return text_options_.default_baseline_adjustment;
    }


    double SVGConverter::measureTextWidth(const std::string& text, const std::string& font_family, double font_size)
    {
        if (text.empty() || font_size <= 0.0) {
            return 0.0;
        }

        std::string primary = primaryFontFromStack(font_family);
        auto font = acquireFontForFamily(primary);

        if (!font || !font->valid) {
            // Fallback: estimate based on character count
            // Average character width is roughly 0.6 of font size for most fonts
            return static_cast<double>(text.length()) * font_size * 0.6;
        }

        double scale = stbtt_ScaleForPixelHeight(font->info.get(), static_cast<float>(font_size));
        double width = 0.0;

        // Use proper UTF-8 codepoint iteration instead of byte-level iteration
        // to correctly measure multi-byte characters
        std::vector<uint32_t> codepoints = utf8ToCodepoints(text);
        for (uint32_t cp : codepoints) {
            int advanceWidth = 0;
            int leftBearing = 0;
            stbtt_GetCodepointHMetrics(font->info.get(),
                                        static_cast<int>(cp),
                                        &advanceWidth, &leftBearing);
            width += static_cast<double>(advanceWidth) * scale;
        }

        return width;
    }

void SVGConverter::initializeDefaultFontOptions()
    {
        if (text_options_.font_fallback_stack.empty())
        {
            text_options_.font_fallback_stack = {"Noto Sans", "Arial", "Helvetica", "sans-serif"};
        }

        if (text_options_.font_baseline_adjustments.empty())
        {
            static const std::pair<const char *, double> kDefaultAdjustments[] = {
                {"Arial", -0.005},
                {"Helvetica", -0.008},
                {"Helvetica Neue", -0.008},
                {"Noto Sans", -0.004},
                {"Noto Sans Mono", -0.010},
                {"ISOCP", -0.060},
                {"ISOCPEUR", -0.060},
                {"ISO CP", -0.060},
                {"ISOCP Europe", -0.060}
            };

            for (const auto &entry : kDefaultAdjustments)
            {
                text_options_.font_baseline_adjustments.emplace(entry.first, entry.second);
            }
        }

        if (std::fabs(text_options_.default_baseline_adjustment) < 1e-9)
        {
            text_options_.default_baseline_adjustment = -0.01;
        }
    }

    static uint16_t readU16BE(const uint8_t *ptr)
    {
        return static_cast<uint16_t>(ptr[0] << 8 | ptr[1]);
    }

    static uint32_t readU32BE(const uint8_t *ptr)
    {
        return (static_cast<uint32_t>(ptr[0]) << 24) |
               (static_cast<uint32_t>(ptr[1]) << 16) |
               (static_cast<uint32_t>(ptr[2]) << 8) |
               static_cast<uint32_t>(ptr[3]);
    }

    static void writeU16BE(uint8_t *dest, uint16_t value)
    {
        dest[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
        dest[1] = static_cast<uint8_t>(value & 0xFF);
    }

    static void writeU32BE(uint8_t *dest, uint32_t value)
    {
        dest[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
        dest[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
        dest[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
        dest[3] = static_cast<uint8_t>(value & 0xFF);
    }

    static uint32_t align4(uint32_t value)
    {
        return (value + 3u) & ~3u;
    }

    static bool convertWoffToSfnt(const std::vector<unsigned char> &woffData,
                                  std::vector<unsigned char> &sfntOut)
    {
        if (woffData.size() < 44)
        {
            return false;
        }

        const uint8_t *data = woffData.data();
        if (readU32BE(data) != 0x774F4646u) // "wOFF"
        {
            return false;
        }

        uint32_t flavor = readU32BE(data + 4);
        uint32_t declaredLength = readU32BE(data + 8);
        if (declaredLength > woffData.size())
        {
            declaredLength = static_cast<uint32_t>(woffData.size());
        }

        uint16_t numTables = readU16BE(data + 12);
        if (numTables == 0)
        {
            return false;
        }

        uint32_t totalSfntSize = readU32BE(data + 16);

        if (44u + static_cast<uint32_t>(numTables) * 20u > woffData.size())
        {
            return false;
        }

        struct SfntTable
        {
            uint32_t tag;
            uint32_t checksum;
            std::vector<unsigned char> bytes;
        };

        std::vector<SfntTable> tables;
        tables.reserve(numTables);

        const uint8_t *dir = data + 44;
        for (uint16_t i = 0; i < numTables; ++i)
        {
            const uint8_t *entry = dir + static_cast<size_t>(i) * 20u;
            uint32_t tag = readU32BE(entry + 0);
            uint32_t offset = readU32BE(entry + 4);
            uint32_t compLength = readU32BE(entry + 8);
            uint32_t origLength = readU32BE(entry + 12);
            uint32_t checksum = readU32BE(entry + 16);

            if (origLength == 0)
            {
                return false;
            }
            if (offset + compLength > woffData.size())
            {
                return false;
            }

            const uint8_t *compData = data + offset;
            SfntTable table;
            table.tag = tag;
            table.checksum = checksum;
            table.bytes.resize(origLength);

            if (compLength == origLength)
            {
                std::memcpy(table.bytes.data(), compData, origLength);
            }
            else
            {
                mz_ulong destLen = origLength;
                int status = mz_uncompress(table.bytes.data(), &destLen, compData, compLength);
                if (status != MZ_OK || destLen != origLength)
                {
                    return false;
                }
            }

            tables.push_back(std::move(table));
        }

        std::sort(tables.begin(), tables.end(), [](const SfntTable &a, const SfntTable &b) {
            return a.tag < b.tag;
        });

        uint32_t headerSize = 12u + static_cast<uint32_t>(numTables) * 16u;
        uint64_t requiredSize64 = headerSize;
        for (const auto &table : tables)
        {
            requiredSize64 += align4(static_cast<uint32_t>(table.bytes.size()));
        }

        if (requiredSize64 > 0xFFFFFFFFull)
        {
            return false;
        }
        uint32_t requiredSize = static_cast<uint32_t>(requiredSize64);
        if (totalSfntSize < requiredSize)
        {
            totalSfntSize = requiredSize;
        }

        sfntOut.assign(totalSfntSize, 0);
        uint8_t *out = sfntOut.data();

        writeU32BE(out, flavor);
        writeU16BE(out + 4, numTables);

        uint16_t maxPow2 = 1;
        uint16_t log2Val = 0;
        while ((maxPow2 << 1) <= numTables)
        {
            maxPow2 <<= 1;
            ++log2Val;
        }
        uint16_t searchRange = static_cast<uint16_t>(maxPow2 * 16);
        uint16_t entrySelector = log2Val;
        uint16_t rangeShift = static_cast<uint16_t>(numTables * 16 - searchRange);
        writeU16BE(out + 6, searchRange);
        writeU16BE(out + 8, entrySelector);
        writeU16BE(out + 10, rangeShift);

        uint32_t recordOffset = 12u;
        uint32_t dataOffset = headerSize;

        for (const auto &table : tables)
        {
            writeU32BE(out + recordOffset, table.tag);
            writeU32BE(out + recordOffset + 4, table.checksum);
            writeU32BE(out + recordOffset + 8, dataOffset);
            writeU32BE(out + recordOffset + 12, static_cast<uint32_t>(table.bytes.size()));

            std::memcpy(out + dataOffset, table.bytes.data(), table.bytes.size());
            dataOffset += align4(static_cast<uint32_t>(table.bytes.size()));
            recordOffset += 16u;
        }

        if (dataOffset < sfntOut.size())
        {
            sfntOut.resize(dataOffset);
        }

        return true;
    }

    std::shared_ptr<LoadedFont> SVGConverter::loadFontFromPath(const std::string &fontPath)
    {
        if (fontPath.empty())
        {
            return nullptr;
        }

        std::filesystem::path resolved(fontPath);
        if (resolved.is_relative())
        {
            if (!cgm_base_dir_.empty())
            {
                resolved = std::filesystem::path(cgm_base_dir_) / resolved;
            }
            else
            {
                resolved = std::filesystem::absolute(resolved);
            }
        }

        resolved = resolved.lexically_normal();
        std::string key = resolved.string();

        auto cacheIt = font_cache_.find(key);
        if (cacheIt != font_cache_.end())
        {
            if (cacheIt->second && cacheIt->second->valid)
            {
                return cacheIt->second;
            }
            return nullptr;
        }

        auto loaded = std::make_shared<LoadedFont>();

        std::ifstream stream(resolved, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            loaded->valid = false;
            font_cache_.emplace(key, loaded);
            return nullptr;
        }

        std::streamsize size = stream.tellg();
        stream.seekg(0, std::ios::beg);
        if (size <= 0)
        {
            loaded->valid = false;
            font_cache_.emplace(key, loaded);
            return nullptr;
        }

        std::vector<unsigned char> fileBytes(static_cast<size_t>(size));
        if (!stream.read(reinterpret_cast<char *>(fileBytes.data()), size))
        {
            loaded->valid = false;
            font_cache_.emplace(key, loaded);
            return nullptr;
        }

        std::vector<unsigned char> sfntData;
        std::string ext = opencgm::utils::toLower(resolved.extension().string());
        if (ext == ".woff")
        {
            if (!convertWoffToSfnt(fileBytes, sfntData))
            {
                std::cerr << "[svg] Failed to convert WOFF font \"" << resolved.string() << "\" to SFNT\n";
                loaded->valid = false;
                font_cache_.emplace(key, loaded);
                return nullptr;
            }
        }
        else if (ext == ".woff2")
        {
            static bool woff2WarningEmitted = false;
            if (!woff2WarningEmitted)
            {
                std::cerr << "[svg] WOFF2 fonts are not currently supported for text-as-path conversion\n";
                woff2WarningEmitted = true;
            }
            loaded->valid = false;
            font_cache_.emplace(key, loaded);
            return nullptr;
        }
        else
        {
            sfntData = std::move(fileBytes);
        }

        if (sfntData.empty())
        {
            loaded->valid = false;
            font_cache_.emplace(key, loaded);
            return nullptr;
        }

        loaded->data = std::move(sfntData);

        int offset = stbtt_GetFontOffsetForIndex(loaded->data.data(), 0);
        if (offset < 0)
        {
            loaded->data.clear();
            loaded->valid = false;
            font_cache_.emplace(key, loaded);
            return nullptr;
        }

        loaded->info = std::make_unique<stbtt_fontinfo>();
        if (!stbtt_InitFont(loaded->info.get(), loaded->data.data(), offset))
        {
            loaded->data.clear();
            loaded->valid = false;
            font_cache_.emplace(key, loaded);
            return nullptr;
        }

        loaded->valid = true;
        font_cache_.emplace(key, loaded);
        return loaded;
    }

    std::shared_ptr<LoadedFont> SVGConverter::acquireFontForFamily(const std::string &fontFamily)
    {
        std::string canonical = fontFamily;
        if (canonical.empty())
        {
            canonical = "default";
        }

        auto cached = font_lookup_.find(canonical);
        if (cached != font_lookup_.end())
        {
            return cached->second;
        }

        auto findPathInsensitive = [&](const std::string &name) -> std::optional<std::string> {
            auto direct = text_options_.font_file_paths.find(name);
            if (direct != text_options_.font_file_paths.end())
            {
                return direct->second;
            }
            std::string lower = opencgm::utils::toLower(name);
            for (const auto &entry : text_options_.font_file_paths)
            {
                if (opencgm::utils::toLower(entry.first) == lower)
                {
                    return entry.second;
                }
            }
            return std::nullopt;
        };

        std::optional<std::string> fontPath = findPathInsensitive(canonical);
        if (!fontPath)
        {
            if (!text_options_.fallback_font_path.empty())
            {
                fontPath = text_options_.fallback_font_path;
            }
        }

        if (!fontPath)
        {
            font_lookup_[canonical] = nullptr;
            return nullptr;
        }

        std::shared_ptr<LoadedFont> font = loadFontFromPath(*fontPath);
        font_lookup_[canonical] = font;
        return font;
    }

    std::vector<std::string> SVGConverter::buildFontStack(const std::string &raw) const
    {
        std::vector<std::string> stack;
        std::string trimmed = opencgm::utils::trimString(raw);
        if (trimmed.empty())
        {
            if (!text_options_.font_fallback_stack.empty())
            {
                return text_options_.font_fallback_stack;
            }
            return {"Arial", "Helvetica", "sans-serif"};
        }

        auto appendIfMissing = [&](const std::string &candidate) {
            if (candidate.empty())
            {
                return;
            }
            auto equalsIgnoreCase = [](const std::string &lhs, const std::string &rhs) {
                if (lhs.size() != rhs.size())
                {
                    return false;
                }
                for (size_t i = 0; i < lhs.size(); ++i)
                {
                    unsigned char a = static_cast<unsigned char>(lhs[i]);
                    unsigned char b = static_cast<unsigned char>(rhs[i]);
                    if (std::tolower(a) != std::tolower(b))
                    {
                        return false;
                    }
                }
                return true;
            };

            for (const auto &existing : stack)
            {
                if (equalsIgnoreCase(existing, candidate))
                {
                    return;
                }
            }
            stack.push_back(candidate);
        };

        std::string lower = trimmed;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        appendIfMissing(trimmed);

        const bool isMonospace = lower.find("courier") != std::string::npos ||
                                 lower.find("mono") != std::string::npos ||
                                 lower.find("fixed") != std::string::npos;
        const bool isSerif = lower.find("times") != std::string::npos ||
                             lower.find("roman") != std::string::npos ||
                             lower.find("serif") != std::string::npos;
        const bool isSans = lower.find("helvetica") != std::string::npos ||
                            lower.find("arial") != std::string::npos ||
                            lower.find("gothic") != std::string::npos ||
                            lower.find("sans") != std::string::npos;
        const bool isScript = lower.find("script") != std::string::npos ||
                              lower.find("cursive") != std::string::npos;
        const bool isSymbol = lower.find("symbol") != std::string::npos ||
                              lower.find("zapf") != std::string::npos ||
                              lower.find("wingdings") != std::string::npos;

        if (isMonospace)
        {
            appendIfMissing("Courier New");
            appendIfMissing("Courier");
            appendIfMissing("monospace");
        }
        if (isSerif)
        {
            appendIfMissing("Times New Roman");
            appendIfMissing("Times");
            appendIfMissing("serif");
        }
        if (isSans)
        {
            appendIfMissing("Arial");
            appendIfMissing("Helvetica");
            appendIfMissing("sans-serif");
        }
        if (isScript)
        {
            appendIfMissing("cursive");
        }
        if (isSymbol)
        {
            appendIfMissing("Symbol");
        }

        if (!isMonospace && !isSerif && !isSans)
        {
            if (!text_options_.font_fallback_stack.empty())
            {
                for (const auto &name : text_options_.font_fallback_stack)
                {
                    appendIfMissing(name);
                }
            }
            else
            {
                appendIfMissing("Arial");
                appendIfMissing("Helvetica");
                appendIfMissing("sans-serif");
            }
        }

        return stack;
    }

    std::string SVGConverter::formatFontStack(const std::vector<std::string> &stack)
    {
        std::ostringstream oss;
        for (size_t i = 0; i < stack.size(); ++i)
        {
            if (i > 0)
            {
                oss << ", ";
            }
            const std::string &name = stack[i];
            bool needsQuotes = name.find(' ') != std::string::npos;
            if (needsQuotes)
            {
                oss << "'" << name << "'";
            }
            else
            {
                oss << name;
            }
        }
        return oss.str();
    }

    std::string SVGConverter::ensureSymbolDefinition(const std::string &name)
    {
        auto it = symbol_definition_ids_.find(name);
        if (it != symbol_definition_ids_.end())
        {
            return it->second;
        }

        std::string baseId = svg::sanitizeIdentifier(name);
        if (baseId.empty())
        {
            baseId = "symbol";
        }
        std::string defId = "cgm-symbol-" + baseId;
        int counter = 1;
        while (emitted_symbol_defs_.count(defId))
        {
            defId = "cgm-symbol-" + baseId + "-" + std::to_string(counter++);
        }

        symbol_definition_ids_[name] = defId;
        emitted_symbol_defs_.insert(defId);

        if (!defs_open_)
        {
            svg_output_ << "  <defs>\n";
            defs_open_ = true;
        }

        std::string sourcePath;
        auto resolvedContent = loadSymbolDefinitionContent(name, sourcePath);
        if (!sourcePath.empty())
        {
            symbol_definition_sources_[name] = sourcePath;
        }

        svg_output_ << "    <g id=\"" << defId << "\" class=\"cgm-symbol-def\"";
        if (!sourcePath.empty())
        {
            svg_output_ << " data-cgm-symbol-source=\"" << escapeXmlAttribute(sourcePath) << "\"";
        }
        auto fragmentIt = symbol_definition_fragments_.find(name);
        if (fragmentIt != symbol_definition_fragments_.end() && !fragmentIt->second.empty())
        {
            svg_output_ << " data-cgm-symbol-fragment=\"" << escapeXmlAttribute(fragmentIt->second) << "\"";
        }
        svg_output_ << ">\n";
        svg_output_ << "      <title>" << escapeXmlText(name) << "</title>\n";

        if (resolvedContent && !resolvedContent->empty())
        {
            svg_output_ << *resolvedContent;
            if (!resolvedContent->empty() && resolvedContent->back() != '\n')
            {
                svg_output_ << "\n";
            }
        }
        else
        {
            svg_output_ << "      <rect x=\"-5\" y=\"-5\" width=\"10\" height=\"10\" fill=\"none\" stroke=\"#666666\" stroke-dasharray=\"2 2\" />\n";
        }

        svg_output_ << "    </g>\n";

        return defId;
    }

    std::optional<std::string> SVGConverter::loadSymbolDefinitionContent(const std::string &name, std::string &sourcePath)
    {
        sourcePath.clear();

        SymbolLibraryDescriptor descriptor = parseSymbolDescriptor(name);

        if (!descriptor.fragment.empty())
        {
            symbol_definition_fragments_[name] = descriptor.fragment;
        }
        else
        {
            symbol_definition_fragments_.erase(name);
        }

        if (symbol_library_paths_.size() < symbol_libraries_.size())
        {
            symbol_library_paths_.resize(symbol_libraries_.size());
        }

        size_t descriptorIndex = 0;
        bool hasIndex = false;
        for (size_t i = 0; i < symbol_libraries_.size(); ++i)
        {
            if (symbol_libraries_[i] == name)
            {
                descriptorIndex = i;
                hasIndex = true;
                break;
            }
        }

        std::string candidatePath;
        if (hasIndex && descriptorIndex < symbol_library_paths_.size())
        {
            candidatePath = symbol_library_paths_[descriptorIndex];
        }

        if (candidatePath.empty())
        {
            if (auto resolved = resolveSymbolLibraryPath(descriptor, cgm_base_dir_))
            {
                candidatePath = *resolved;
                if (hasIndex)
                {
                    symbol_library_paths_[descriptorIndex] = candidatePath;
                }
            }
            else if (!descriptor.uri.empty())
            {
                candidatePath = descriptor.uri;
                if (hasIndex)
                {
                    symbol_library_paths_[descriptorIndex] = candidatePath;
                }
            }
        }

        if (candidatePath.empty())
        {
            return std::nullopt;
        }

        if (isLikelyRemoteUri(candidatePath))
        {
            sourcePath = candidatePath;
            if (!descriptor.fragment.empty() && sourcePath.find('#') == std::string::npos)
            {
                sourcePath += "#" + descriptor.fragment;
            }
            return std::nullopt;
        }

        std::ifstream stream(candidatePath, std::ios::binary);
        if (!stream)
        {
            sourcePath = candidatePath;
            if (!descriptor.fragment.empty() && sourcePath.find('#') == std::string::npos)
            {
                sourcePath += "#" + descriptor.fragment;
            }
            return std::nullopt;
        }

        std::string rawContent((std::istreambuf_iterator<char>(stream)),
                               std::istreambuf_iterator<char>());
        sourcePath = candidatePath;
        if (!descriptor.fragment.empty() && sourcePath.find('#') == std::string::npos)
        {
            sourcePath += "#" + descriptor.fragment;
        }

        if (rawContent.empty())
        {
            return std::nullopt;
        }

        auto markup = buildSymbolMarkup(rawContent, descriptor);
        if (!markup)
        {
            return std::nullopt;
        }

        return indentMultiline(*markup, 6);
    }

    void SVGConverter::processSymbolLibraryList(SymbolLibraryList *cmd)
    {
        if (!cmd)
        {
            symbol_libraries_.clear();
            symbol_library_paths_.clear();
            symbol_definition_sources_.clear();
            symbol_definition_fragments_.clear();
            return;
        }

        symbol_libraries_ = cmd->libraries();
        symbol_library_paths_.assign(symbol_libraries_.size(), std::string());
        symbol_definition_sources_.clear();
        symbol_definition_fragments_.clear();
        symbol_definition_ids_.clear();
    }

    void SVGConverter::processMaximumColourIndex(MaximumColourIndex *cmd)
    {
        if (!cmd)
        {
            return;
        }

        int maxIndex = cmd->maxIndex();
        if (maxIndex >= 0)
        {
            expected_colour_table_size_ = static_cast<size_t>(maxIndex) + 1;
        }
        else
        {
            expected_colour_table_size_ = 0;
        }
    }

    void SVGConverter::processColourValueExtent(ColourValueExtent *cmd)
    {
        if (!cmd)
        {
            colour_value_extent_min_ = Color::Black();
            colour_value_extent_max_ = Color::White();
        }
        else
        {
            colour_value_extent_min_ = cmd->minColor().color();
            colour_value_extent_max_ = cmd->maxColor().color();
        }

        if (color_logging_enabled_)
        {
            std::cerr << "[color] extent min=("
                      << static_cast<int>(colour_value_extent_min_.r) << ","
                      << static_cast<int>(colour_value_extent_min_.g) << ","
                      << static_cast<int>(colour_value_extent_min_.b) << ") max=("
                      << static_cast<int>(colour_value_extent_max_.r) << ","
                      << static_cast<int>(colour_value_extent_max_.g) << ","
                      << static_cast<int>(colour_value_extent_max_.b) << ")\n";
        }

        for (auto &entry : color_table_)
        {
            entry.second = applyColourValueExtent(entry.second);
        }
    }

    Color SVGConverter::applyColourValueExtent(const Color &value) const
    {
        // ISO 8632-1 §6.4.10: scale each channel from [min, max] (per CVE)
        // back to [0, 255] for SVG. With CVE at default (0..255), this is
        // a no-op — the channel values are already literal 8-bit RGB.
        // (Removed: a maxComponent<=100 "percent rescale" heuristic that
        // wrongly mapped legitimate dark colours like (100,100,100) to
        // (255,255,255), corrupting COLLVL03's grayscale colour table.)

        return svg::ColorResolver::applyValueExtent(
            value,
            colour_value_extent_min_,
            colour_value_extent_max_);
    }

    void SVGConverter::processFontList(FontList *cmd)
    {
        if (!cmd)
        {
            font_list_.clear();
            current_style_.setFontFamily(resolveFontFamilyFromIndex(current_style_.fontIndex()));
            return;
        }

        font_list_ = cmd->fonts();
        current_style_.setFontFamily(resolveFontFamilyFromIndex(current_style_.fontIndex()));
    }

    void SVGConverter::processPolymarker(Polymarker *cmd)
    {
        if (!cmd || cmd->points().empty())
            return;

        const auto &points = cmd->points();
        int markerType = current_style_.markerType();
        double markerSize = current_style_.markerSize();
        Color markerColor = current_style_.markerColor();

        // Transform marker size to SVG coordinates
        double svg_size = transform_.transformLength(markerSize);

        // Generate marker path data
        std::string markerPath = getMarkerPathData(markerType, svg_size);

        // Output each marker as a path element at each point
        for (const auto &pt : points)
        {
            CGMPoint svg_pt = transform_.transformPoint(pt);

            svg_output_ << "  <path d=\"";

            // Translate the marker path to the point location
            svg_output_ << "M " << svg_pt.x() << " " << svg_pt.y() << " ";
            svg_output_ << markerPath;

            // Convert color to hex
            char hex[8];
            snprintf(hex, sizeof(hex), "#%02X%02X%02X",
                     static_cast<int>(markerColor.r),
                     static_cast<int>(markerColor.g),
                     static_cast<int>(markerColor.b));
            std::string colorHex(hex);

            svg_output_ << "\" fill=\"" << colorHex << "\" ";
            svg_output_ << "stroke=\"" << colorHex << "\" ";
            svg_output_ << "stroke-width=\"" << (svg_size * 0.1) << "\" />\n";
        }
    }

    std::string SVGConverter::getMarkerPathData(int markerType, double size)
    {
        std::ostringstream path;
        double half = size / 2.0;

        switch (markerType)
        {
        case 1: // DOT
            // Small filled circle
            path << "m " << (-half * 0.3) << " 0 ";
            path << "a " << (half * 0.3) << " " << (half * 0.3) << " 0 1 0 " << (half * 0.6) << " 0 ";
            path << "a " << (half * 0.3) << " " << (half * 0.3) << " 0 1 0 " << (-half * 0.6) << " 0 z";
            break;

        case 2: // PLUS
            // + shape
            path << "m " << (-half) << " 0 l " << size << " 0 ";
            path << "m " << (-half) << " " << (-half) << " l 0 " << size;
            break;

        case 3: // ASTERISK
            // * shape (6 lines from center)
            for (int i = 0; i < 6; ++i)
            {
                double angle = i * 60.0 * M_PI / 180.0;
                double dx = half * std::cos(angle);
                double dy = half * std::sin(angle);
                path << "m 0 0 l " << dx << " " << dy << " ";
                path << "m " << (-dx) << " " << (-dy) << " ";
            }
            break;

        case 4: // CIRCLE
            // Hollow circle
            path << "m " << (-half) << " 0 ";
            path << "a " << half << " " << half << " 0 1 0 " << size << " 0 ";
            path << "a " << half << " " << half << " 0 1 0 " << (-size) << " 0 z";
            break;

        case 5: // CROSS (X)
            // X shape (diagonal)
            path << "m " << (-half) << " " << (-half) << " l " << size << " " << size << " ";
            path << "m 0 " << (-size) << " l " << (-size) << " " << size;
            break;

        default:
            // Default to DOT if unknown type
            path << "m " << (-half * 0.3) << " 0 ";
            path << "a " << (half * 0.3) << " " << (half * 0.3) << " 0 1 0 " << (half * 0.6) << " 0 ";
            path << "a " << (half * 0.3) << " " << (half * 0.3) << " 0 1 0 " << (-half * 0.6) << " 0 z";
            break;
        }

        return path.str();
    }

    void SVGConverter::processMarkerType(MarkerType *cmd)
    {
        if (!cmd)
            return;

        current_style_.setMarkerType(cmd->type());
    }

    void SVGConverter::processMarkerSize(MarkerSize *cmd)
    {
        if (!cmd)
            return;

        current_style_.setMarkerSize(cmd->size());
    }

    void SVGConverter::processMarkerColor(MarkerColour *cmd)
    {
        if (!cmd)
            return;

        current_style_.setMarkerColor(resolveColor(cmd->color(), ColorRole::Marker, "MARKER COLOUR"));
        current_style_.markMarkerColorExplicit();
    }

    void SVGConverter::processLineCap(LineCap *cmd)
    {
        if (!cmd)
            return;

        current_style_.setLineCap(cmd->lineIndicator());
    }

    void SVGConverter::processLineJoin(LineJoin *cmd)
    {
        if (!cmd)
            return;

        current_style_.setLineJoin(cmd->type());
    }

    void SVGConverter::processMitreLimit(MitreLimit *cmd)
    {
        if (!cmd)
            return;

        current_style_.setMiterLimit(cmd->limit());
    }

    void SVGConverter::processEdgeCap(EdgeCap *cmd)
    {
        if (!cmd)
            return;

        current_style_.setEdgeCap(cmd->lineIndicator());
    }

    void SVGConverter::processEdgeJoin(EdgeJoin *cmd)
    {
        if (!cmd)
            return;

        current_style_.setEdgeJoin(cmd->type());
    }

    void SVGConverter::processEscape(Escape *cmd)
    {
        if (!cmd)
        {
            return;
        }

        int id = cmd->identifier();
        const std::string &dataRecord = cmd->dataRecord();

        // WebCGM 2.1 alpha transparency escape
        // Identifier 45 is used in practice (binary encoding of escape -1)
        // Some implementations may use -1 directly
        if (id == -1 || id == 45)
        {
            // Parse the opacity value from the data record
            // Format: Binary data with IEEE 754 floating point value
            // Observed format: 00 0c 00 01 [4-byte IEEE 754 float]
            if (dataRecord.size() >= 8)
            {
                // Extract last 4 bytes as IEEE 754 32-bit float (big-endian)
                unsigned char b0 = static_cast<unsigned char>(dataRecord[4]);
                unsigned char b1 = static_cast<unsigned char>(dataRecord[5]);
                unsigned char b2 = static_cast<unsigned char>(dataRecord[6]);
                unsigned char b3 = static_cast<unsigned char>(dataRecord[7]);

                // Combine bytes into 32-bit integer (big-endian)
                uint32_t bits = (static_cast<uint32_t>(b0) << 24) |
                                (static_cast<uint32_t>(b1) << 16) |
                                (static_cast<uint32_t>(b2) << 8) |
                                 static_cast<uint32_t>(b3);

                // Reinterpret as float
                float opacity_f;
                std::memcpy(&opacity_f, &bits, sizeof(float));
                double opacity = static_cast<double>(opacity_f);

                current_style_.setOpacity(opacity);
            }
        }
        // Registered escape -301: Line cap override
        else if (id == -301)
        {
            if (dataRecord.size() >= 2)
            {
                int capValue = static_cast<unsigned char>(dataRecord[0]);
                // CGM Line Cap values:
                // 1 = Unspecified, 2 = Butt, 3 = Round, 4 = Projecting square, 5 = Triangle
                switch (capValue)
                {
                    case 2: current_style_.setLineCap(LineCapIndicator::BUTT); break;
                    case 3: current_style_.setLineCap(LineCapIndicator::ROUND); break;
                    case 4: current_style_.setLineCap(LineCapIndicator::PROJECTING_SQUARE); break;
                    case 5: current_style_.setLineCap(LineCapIndicator::TRIANGLE); break;
                    default: current_style_.setLineCap(LineCapIndicator::UNSPECIFIED); break;
                }
            }
        }
        // Registered escape -302: Line join override
        else if (id == -302)
        {
            if (dataRecord.size() >= 2)
            {
                int joinValue = static_cast<unsigned char>(dataRecord[0]);
                // CGM Line Join values:
                // 1 = Unspecified, 2 = Miter, 3 = Round, 4 = Bevel
                switch (joinValue)
                {
                    case 2: current_style_.setLineJoin(JoinIndicator::MITER); break;
                    case 3: current_style_.setLineJoin(JoinIndicator::ROUND); break;
                    case 4: current_style_.setLineJoin(JoinIndicator::BEVEL); break;
                    default: current_style_.setLineJoin(JoinIndicator::UNSPECIFIED); break;
                }
            }
        }
        // Registered escape -303: Miter limit override
        else if (id == -303)
        {
            if (dataRecord.size() >= 4)
            {
                // Parse 4-byte IEEE 754 float (big-endian)
                unsigned char b0 = static_cast<unsigned char>(dataRecord[0]);
                unsigned char b1 = static_cast<unsigned char>(dataRecord[1]);
                unsigned char b2 = static_cast<unsigned char>(dataRecord[2]);
                unsigned char b3 = static_cast<unsigned char>(dataRecord[3]);

                uint32_t bits = (static_cast<uint32_t>(b0) << 24) |
                                (static_cast<uint32_t>(b1) << 16) |
                                (static_cast<uint32_t>(b2) << 8) |
                                 static_cast<uint32_t>(b3);

                float miter_f;
                std::memcpy(&miter_f, &bits, sizeof(float));
                double miterLimit = static_cast<double>(miter_f);

                // Clamp to reasonable values (SVG default is 4)
                if (miterLimit > 0.0 && miterLimit < 100.0)
                {
                    current_style_.setMiterLimit(miterLimit);
                }
            }
        }
    }

    void SVGConverter::emitSampledPolyline(const std::vector<CGMPoint> &points)
    {
        if (points.size() < 2)
        {
            return;
        }

        std::vector<CGMPoint> transformed;
        transformed.reserve(points.size());

        for (const auto &pt : points)
        {
            if (!std::isfinite(pt.x()) || !std::isfinite(pt.y()))
            {
                continue;
            }
            transformed.push_back(transform_.transformPoint(pt));
        }

        if (transformed.size() < 2)
        {
            return;
        }

        bool hasExtent = false;
        for (size_t i = 1; i < transformed.size(); ++i)
        {
            double dx = transformed[i].x() - transformed[i - 1].x();
            double dy = transformed[i].y() - transformed[i - 1].y();
            if (std::hypot(dx, dy) > 1e-9)
            {
                hasExtent = true;
                break;
            }
        }

        if (!hasExtent)
        {
            return;
        }

        const double duplicateTolSq = 1e-6;
        const double colinearTolSq = 0.25 * 0.25; // quarter pixel tolerance

        std::vector<CGMPoint> simplified;
        simplified.reserve(transformed.size());
        simplified.push_back(transformed.front());

        for (size_t i = 1; i + 1 < transformed.size(); ++i)
        {
            const CGMPoint &prev = simplified.back();
            const CGMPoint &current = transformed[i];
            const CGMPoint &next = transformed[i + 1];

            if (distanceSquared(current, prev) <= duplicateTolSq)
            {
                continue;
            }

            double deviationSq = distancePointToSegmentSquared(current, prev, next);
            if (deviationSq <= colinearTolSq)
            {
                continue;
            }

            simplified.push_back(current);
        }

        const CGMPoint &last = transformed.back();
        if (distanceSquared(last, simplified.back()) > duplicateTolSq)
        {
            simplified.push_back(last);
        }

        if (simplified.size() < 2)
        {
            return;
        }

        svg_output_ << "  <path d=\"M " << simplified.front().x() << " " << simplified.front().y();
        for (size_t i = 1; i < simplified.size(); ++i)
        {
            svg_output_ << " L " << simplified[i].x() << " " << simplified[i].y();
        }
        svg_output_ << "\" " << current_style_.getStrokeStyle()
                    << "fill=\"none\" />\n";
    }

    std::string SVGConverter::encodeCellArrayToPng(const std::vector<std::vector<Color>> &colors, int width, int height)
    {
#ifdef _WIN32
        if (width <= 0 || height <= 0)
        {
            return {};
        }

        IWICImagingFactory *factory = getWicFactory();
        if (!factory)
        {
            return {};
        }

        std::vector<uint8_t> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
        for (int y = 0; y < height; ++y)
        {
            const auto &row = colors[static_cast<size_t>(y)];
            for (int x = 0; x < width; ++x)
            {
                Color sample = (x < static_cast<int>(row.size())) ? row[static_cast<size_t>(x)] : Color::White();
                size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
                rgba[idx + 0] = sample.r;
                rgba[idx + 1] = sample.g;
                rgba[idx + 2] = sample.b;
                rgba[idx + 3] = sample.a;
            }
        }

        if (png_quantization_enabled_)
        {
            auto quantizeChannel = [](uint8_t value) -> uint8_t {
                uint8_t upper = static_cast<uint8_t>(value & 0xF0);
                return static_cast<uint8_t>(upper | (upper >> 4));
            };
            const size_t totalPixels = static_cast<size_t>(width) * static_cast<size_t>(height);
            for (size_t i = 0; i < totalPixels; ++i)
            {
                size_t base = i * 4;
                rgba[base + 0] = quantizeChannel(rgba[base + 0]);
                rgba[base + 1] = quantizeChannel(rgba[base + 1]);
                rgba[base + 2] = quantizeChannel(rgba[base + 2]);
                // Alpha preserved
            }
        }

        auto encoded = encodeRgbaToPng(factory, rgba, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        if (!encoded)
        {
            return {};
        }

        return base64Encode(std::string(reinterpret_cast<const char *>(encoded->data()), encoded->size()));
#else
        (void)colors;
        (void)width;
        (void)height;
        return {};
#endif
    }

    std::string SVGConverter::encodeCellArrayToJpeg(const std::vector<std::vector<Color>> &colors, int width, int height, int quality)
    {
#ifdef _WIN32
        if (width <= 0 || height <= 0)
        {
            return {};
        }

        IWICImagingFactory *factory = getWicFactory();
        if (!factory)
        {
            return {};
        }

        std::vector<uint8_t> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
        for (int y = 0; y < height; ++y)
        {
            const auto &row = colors[static_cast<size_t>(y)];
            for (int x = 0; x < width; ++x)
            {
                Color sample = (x < static_cast<int>(row.size())) ? row[static_cast<size_t>(x)] : Color::White();
                size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
                rgba[idx + 0] = sample.r;
                rgba[idx + 1] = sample.g;
                rgba[idx + 2] = sample.b;
                rgba[idx + 3] = 255;  // JPEG doesn't support alpha, use opaque
            }
        }

        auto encoded = encodeRgbaToJpeg(factory, rgba, static_cast<uint32_t>(width), static_cast<uint32_t>(height), quality);
        if (!encoded)
        {
            return {};
        }

        return base64Encode(std::string(reinterpret_cast<const char *>(encoded->data()), encoded->size()));
#else
        (void)colors;
        (void)width;
        (void)height;
        (void)quality;
        return {};
#endif
    }

    std::string SVGConverter::encodeCellArrayToBmp(const std::vector<std::vector<Color>> &colors, int width, int height)
    {
        if (width <= 0 || height <= 0)
        {
            return {};
        }

        if (static_cast<int>(colors.size()) < height)
        {
            return {};
        }

        const int rowStride = ((width * 3 + 3) / 4) * 4;
        const int pixelDataSize = rowStride * height;
        const int fileHeaderSize = 14;
        const int infoHeaderSize = 40;
        const int totalSize = fileHeaderSize + infoHeaderSize + pixelDataSize;

        std::vector<uint8_t> buffer(static_cast<size_t>(totalSize), 0);

        auto write16 = [&](size_t offset, uint16_t value)
        {
            buffer[offset] = static_cast<uint8_t>(value & 0xFF);
            buffer[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        };

        auto write32 = [&](size_t offset, uint32_t value)
        {
            buffer[offset] = static_cast<uint8_t>(value & 0xFF);
            buffer[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
            buffer[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
            buffer[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
        };

        buffer[0] = 'B';
        buffer[1] = 'M';
        write32(2, static_cast<uint32_t>(totalSize));
        write32(10, static_cast<uint32_t>(fileHeaderSize + infoHeaderSize));

        write32(14, static_cast<uint32_t>(infoHeaderSize));
        write32(18, static_cast<uint32_t>(width));
        write32(22, static_cast<uint32_t>(height));
        write16(26, 1);  // planes
        write16(28, 24); // bits per pixel
        write32(30, 0);  // compression (BI_RGB)
        write32(34, static_cast<uint32_t>(pixelDataSize));
        write32(38, 2835); // 72 DPI
        write32(42, 2835); // 72 DPI

        const size_t pixelOffset = static_cast<size_t>(fileHeaderSize + infoHeaderSize);

        for (int y = 0; y < height; ++y)
        {
            const auto &rowColors = colors[y];
            size_t rowStart = pixelOffset + static_cast<size_t>(y) * rowStride;
            size_t cursor = rowStart;
            for (int x = 0; x < width; ++x)
            {
                Color color = (x < static_cast<int>(rowColors.size())) ? rowColors[x] : Color::White();
                buffer[cursor++] = color.b;
                buffer[cursor++] = color.g;
                buffer[cursor++] = color.r;
            }
            while (cursor < rowStart + rowStride)
            {
                buffer[cursor++] = 0;
            }
        }

        return base64Encode(std::string(reinterpret_cast<const char *>(buffer.data()), buffer.size()));
    }

    void SVGConverter::emitNurbsPath(
        int order,
        const std::vector<CGMPoint> &controlPoints,
        const std::vector<double> &weights,
        const std::vector<double> &sourceKnots,
        double startParam,
        double endParam,
        const char *primitiveName)
    {
        const auto knots = NurbsApproximator::resolveKnotVector(
            sourceKnots,
            order,
            controlPoints.size(),
            startParam,
            endParam);
        if (knots.empty())
        {
            svg_output_ << "  <!-- " << primitiveName
                        << " omitted: invalid order, parameter range, or knot vector"
                        << " (ISO/IEC 8632-1) -->\n";
            return;
        }

        NurbsApproximator approximator;
        NurbsApproximationOptions options;
        const double svgUnit = transform_.transformLength(1.0);
        options.tolerance = svgUnit > 1e-6
            ? nurbs_tolerance_svg_units_ / svgUnit
            : std::abs(endParam - startParam) / 50.0;

        const auto segments = approximator.approximateNurbs(
            order, controlPoints, weights, knots,
            startParam, endParam, options);
        if (segments.empty())
        {
            return;
        }

        const auto transformFunc = [this](const CGMPoint &pt) {
            return transform_.transformPoint(pt);
        };
        const std::string pathData = approximator.generateSvgPath(segments, transformFunc);

        svg_output_ << "  <path d=\"" << pathData << "\" "
                    << current_style_.getStrokeStyle()
                    << "fill=\"none\"" << debugCommandAttribute() << " />\n";
    }

    void SVGConverter::processNonUniformBSpline(NonUniformBSpline *cmd)
    {
        if (!cmd || cmd->controlPoints().empty())
        {
            return;
        }

        emitNurbsPath(
            cmd->splineOrder(),
            cmd->controlPoints(),
            {},
            cmd->knots(),
            cmd->startParameter(),
            cmd->endParameter(),
            "NUBS");
    }

    void SVGConverter::processNonUniformRationalBSpline(NonUniformRationalBSpline *cmd)
    {
        if (!cmd || cmd->controlPoints().empty())
        {
            return;
        }

        emitNurbsPath(
            cmd->splineOrder(),
            cmd->controlPoints(),
            cmd->weights(),
            cmd->knots(),
            cmd->startParameter(),
            cmd->endParameter(),
            "NURBS");
    }

    // ============================================================================
    // APS (Application Structure) Support
    // ============================================================================

    void SVGConverter::processBeginApplicationStructure(BeginApplicationStructure *cmd)
    {
        if (!cmd)
            return;

        // Create APS node
        APSNode node;
        node.identifier = cmd->identifier();
        node.resolved_identifier = aps_id_allocator_.allocate(node.identifier);
        node.type = cmd->type();
        node.inheritanceFlag = cmd->inheritanceFlag();
        node.nesting_level = static_cast<int>(aps_stack_.size());
        node.attributes = current_aps_attributes_; // Collect attributes accumulated before BODY
        if (node.attributes.find("apsid") == node.attributes.end() && !node.identifier.empty())
        {
            node.attributes["apsid"] = node.identifier;
        }

        aps_stack_.push_back(node);

        // Don't output SVG yet - wait for BEGIN APS BODY
    }

    void SVGConverter::processBeginApplicationStructureBody(BeginApplicationStructureBody *cmd)
    {
        (void)cmd;
        if (aps_stack_.empty())
        {
            // Error: BODY without BEGIN
            return;
        }

        // Snapshot attributes collected between BEGIN APS and BODY
        aps_stack_.back().attributes = current_aps_attributes_;
        aps_stack_.back().linkuris = current_aps_linkuris_;  // Copy multi-link entries

        // Now output the opening <g> tag with all collected attributes
        outputAPSOpenTag();

        // Clear attributes for next APS
        current_aps_attributes_.clear();
        current_aps_linkuris_.clear();  // Clear multi-link entries
    }

    void SVGConverter::processEndApplicationStructure(EndApplicationStructure *cmd)
    {
        (void)cmd;
        if (aps_stack_.empty())
        {
            // Error: END without BEGIN
            return;
        }

        // Output closing </g> tag
        outputAPSCloseTag();

        // Pop from stack
        aps_stack_.pop_back();
    }

    void SVGConverter::processApplicationStructureAttribute(ApplicationStructureAttribute *cmd)
    {
        if (!cmd)
            return;

        bool debugAps = false;
#ifndef NDEBUG
#if defined(_MSC_VER)
        char *envAps = nullptr;
        size_t envApsLen = 0;
        if (_dupenv_s(&envAps, &envApsLen, "SVG_DEBUG_APS") == 0 && envAps && *envAps)
        {
            debugAps = true;
        }
        if (envAps)
        {
            free(envAps);
        }
#else
        if (std::getenv("SVG_DEBUG_APS"))
        {
            debugAps = true;
        }
#endif
#endif // NDEBUG

        // Store attribute for the current APS
        // Will be output when BEGIN APS BODY is encountered
        std::string attrType = cmd->attributeType();
        std::string attrData = cmd->data();
        std::string decodedText;
        if (auto structuredOpt = cmd->structuredText())
        {
            decodedText = *structuredOpt;
        }

        for (char &c : attrType)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        if (debugAps)
        {
            std::ostringstream hex;
            hex << std::hex << std::setfill('0');
            for (unsigned char ch : attrData)
            {
                hex << std::setw(2) << static_cast<int>(ch) << ' ';
            }
            std::cerr << "[APS] type=\"" << attrType << "\" raw-bytes=" << attrData.size() << " -> " << hex.str();
            if (!decodedText.empty())
            {
                std::cerr << " | decoded=\"" << decodedText << "\"";
            }
            std::cerr << "\n";
        }

        if (attrType == "region")
        {
            // Preserve raw SDR for region processing (converted later)
            current_aps_attributes_[attrType] = attrData;
            return;
        }

        if (attrType == "viewcontext")
        {
            current_aps_attributes_.erase("viewcontext-nvdc");
            auto assignViewAttributes =
                [&](const svg::ApsRect &rect)
            {
                current_aps_attributes_[attrType] =
                    svg::ApsGeometry::formatRect(rect);
                const auto nvdc = svg::ApsGeometry::toNvdc(
                    rect,
                    scaling_mode_ == SpecificationMode::SCALED,
                    metric_scale_factor_,
                    picture_vdc_min_x_,
                    picture_vdc_min_y_,
                    picture_vdc_max_y_,
                    picture_vdc_y_down_);
                if (nvdc)
                {
                    current_aps_attributes_["viewcontext-nvdc"] =
                        svg::ApsGeometry::formatRect(*nvdc);
                }
            };

            bool treatAsBinary = cmd->isBinaryData() || SDRParser::isBinarySDR(attrData);
            if (treatAsBinary)
            {
                if (auto rect = SDRParser::parseViewContextRect(attrData))
                {
                    if (auto ordered = svg::ApsGeometry::orderedRect(
                            rect->minX,
                            rect->minY,
                            rect->maxX,
                            rect->maxY))
                    {
                        assignViewAttributes(*ordered);
                        return;
                    }
                }
            }

            auto parseTokenRect = [&](const std::vector<std::string> &tokens) -> bool
            {
                const auto rect =
                    svg::ApsGeometry::parseRectTokens(tokens);
                if (!rect)
                {
                    return false;
                }
                assignViewAttributes(*rect);
                return true;
            };

            if (!decodedText.empty())
            {
                auto structuredTokens =
                    svg::ApsTextDecoder::decodeTokens(decodedText);
                if (parseTokenRect(structuredTokens))
                {
                    return;
                }
            }

            auto attrTokens =
                svg::ApsTextDecoder::decodeTokens(attrData);
            if (parseTokenRect(attrTokens))
            {
                return;
            }

            if (!treatAsBinary && SDRParser::isBinarySDR(attrData))
            {
                if (auto rect = SDRParser::parseViewContextRect(attrData))
                {
                    if (auto ordered = svg::ApsGeometry::orderedRect(
                            rect->minX,
                            rect->minY,
                            rect->maxX,
                            rect->maxY))
                    {
                        assignViewAttributes(*ordered);
                        return;
                    }
                }
            }
        }

        auto rawTokens = svg::ApsTextDecoder::decodeTokens(attrData);
        std::vector<std::string> structuredTokens;
        if (!decodedText.empty())
        {
            structuredTokens =
                svg::ApsTextDecoder::decodeTokens(decodedText);
        }

        const std::vector<std::string> *activeTokens = &rawTokens;
        bool usingStructuredTokens = false;

        if (!structuredTokens.empty())
        {
            int rawScore =
                svg::ApsAttributeInterpreter::scoreTokens(rawTokens);
            int structuredScore =
                svg::ApsAttributeInterpreter::scoreTokens(
                    structuredTokens);
            if (structuredScore >= rawScore + 5 || (structuredScore >= 0 && structuredScore >= rawScore))
            {
                activeTokens = &structuredTokens;
                usingStructuredTokens = true;
            }
        }

        const auto &tokens = *activeTokens;

        if (attrType == "linkuri")
        {
            if (debugAps)
            {
                const unsigned char *bytes = reinterpret_cast<const unsigned char *>(attrData.data());
                size_t remaining = attrData.size();
                if (remaining >= 4)
                {
                    uint16_t fieldType = static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
                    uint16_t componentCount = static_cast<uint16_t>((bytes[2] << 8) | bytes[3]);
                    std::cerr << "[linkuri] fieldType=0x" << std::hex << fieldType
                              << " components=" << std::dec << componentCount << "\n";
                    size_t offset = 4;
                    for (uint16_t i = 0; i < componentCount && offset < remaining; ++i)
                    {
                        uint8_t length = bytes[offset++];
                        std::string component;
                        if (offset + length <= remaining)
                        {
                            component.assign(reinterpret_cast<const char *>(bytes + offset), length);
                            offset += length;
                        }
                        else
                        {
                            component = "<truncated>";
                            offset = remaining;
                        }
                        std::cerr << "  component[" << i << "] len=" << static_cast<int>(length)
                                  << " value=\"" << component << "\"\n";
                    }
                }
            }

            auto linkValues =
                svg::ApsAttributeInterpreter::parseLinkuri(tokens);

            if (linkValues.uri.empty())
            {
                std::vector<std::string> fallbackInput;

                if (!structuredTokens.empty())
                {
                    fallbackInput = structuredTokens;
                }

                if (fallbackInput.empty() && !decodedText.empty())
                {
                    std::string sanitizedDecoded =
                        svg::ApsAttributeInterpreter::sanitizeScalar(
                            decodedText,
                            false);
                    if (!sanitizedDecoded.empty())
                    {
                        fallbackInput.push_back(std::move(sanitizedDecoded));
                    }
                }

                if (fallbackInput.empty())
                {
                    auto structuredComponents = SDRParser::decodeLinkuriStructuredData(attrData);
                    bool anyUseful = false;
                    for (auto &component : structuredComponents)
                    {
                        std::string sanitized =
                            svg::ApsAttributeInterpreter::sanitizeScalar(
                                component,
                                false);
                        if (!sanitized.empty())
                        {
                            anyUseful = true;
                        }
                        fallbackInput.push_back(std::move(sanitized));
                    }
                    if (!anyUseful)
                    {
                        fallbackInput.clear();
                    }
                }

                if (fallbackInput.empty() && !attrData.empty())
                {
                    std::string sanitizedRaw =
                        svg::ApsAttributeInterpreter::sanitizeScalar(
                            attrData,
                            false);
                    if (!sanitizedRaw.empty())
                    {
                        fallbackInput.push_back(std::move(sanitizedRaw));
                    }
                }

                if (!fallbackInput.empty())
                {
                    auto fallbackValues =
                        svg::ApsAttributeInterpreter::parseLinkuri(
                            fallbackInput);
                    if (!fallbackValues.uri.empty())
                    {
                        linkValues = std::move(fallbackValues);
                    }
                }
            }

            std::string candidateUri;
            if (!linkValues.uri.empty())
            {
                candidateUri = linkValues.uri;
            }
            else if (!rawTokens.empty())
            {
                std::string fallback =
                    svg::ApsAttributeInterpreter::sanitizeScalar(
                        utils::stripQuotes(
                            utils::trimString(rawTokens.front())));
                if (!fallback.empty())
                {
                    candidateUri =
                        svg::ApsAttributeInterpreter::encodeUri(
                            fallback);
                }
            }

            std::string sanitizedHref =
                svg::ApsAttributeInterpreter::sanitizeLinkHref(
                    candidateUri);
            if (!sanitizedHref.empty())
            {
                if (debugAps && sanitizedHref != candidateUri)
                {
                    std::cerr << "[linkuri] sanitized href=\"" << sanitizedHref << "\" (from \"" << candidateUri << "\")\n";
                }
                linkValues.uri = sanitizedHref;

                // Add to multi-link list for all links
                LinkuriEntry entry;
                entry.uri = sanitizedHref;
                entry.title = linkValues.content;  // Link title from content parameter
                entry.behavior = linkValues.behavior;
                current_aps_linkuris_.push_back(entry);

                // Keep first link in attributes map for backward compatibility
                if (current_aps_attributes_.find(attrType) == current_aps_attributes_.end())
                {
                    current_aps_attributes_[attrType] = sanitizedHref;
                }
            }
            else
            {
                if (debugAps && !candidateUri.empty())
                {
                    std::cerr << "[linkuri] dropped unsafe href \"" << candidateUri << "\"\n";
                }
                // Don't erase - there may be previous valid linkuris
            }

            // Store behavior from first link only in attributes map
            if (current_aps_linkuris_.size() == 1)
            {
                if (!linkValues.behavior.empty())
                {
                    current_aps_attributes_["behavior"] = linkValues.behavior;
                }

                if (!linkValues.target.empty())
                {
                    current_aps_attributes_["target"] = linkValues.target;
                }

                if (!linkValues.content.empty())
                {
                    current_aps_attributes_["content"] = linkValues.content;
                }

                if (!linkValues.highlight.empty())
                {
                    current_aps_attributes_["highlight"] = linkValues.highlight;
                }
            }
            return;
        }

        std::string sanitized;
        auto valueParts =
            svg::ApsAttributeInterpreter::collectValueTokens(
                tokens,
                attrType);
        if (!valueParts.empty())
        {
            sanitized =
                svg::ApsAttributeInterpreter::joinValues(valueParts);
        }
        else
        {
            sanitized =
                svg::ApsAttributeInterpreter::fallbackValue(
                    tokens,
                    attrType);
            if (sanitized.empty() && usingStructuredTokens)
            {
                std::string trimmed = utils::trimString(decodedText);
                sanitized = trimmed;
            }
        }

        sanitized =
            svg::ApsAttributeInterpreter::sanitizeScalar(
                utils::stripQuotes(utils::trimString(sanitized)));
        if (!sanitized.empty())
        {
            current_aps_attributes_[attrType] = sanitized;
        }
        else
        {
            current_aps_attributes_.erase(attrType);
        }
    }

    void SVGConverter::outputAPSOpenTag()
    {
        if (aps_stack_.empty())
            return;

        APSNode &node = aps_stack_.back();

        // Merge XCF attributes if XCF merger is available
        std::map<std::string, std::string> mergedAttributes = node.attributes;
        if (xcf_merger_ != nullptr && xcf_merger_->hasXcfData())
        {
            // Get name attribute if present for XCF lookup
            std::string apsName;
            auto nameIt = node.attributes.find("name");
            if (nameIt != node.attributes.end())
            {
                apsName = nameIt->second;
            }

            // Get merged attributes from XCF
            mergedAttributes = xcf_merger_->getMergedAttributes(
                node.identifier, apsName, node.attributes);
        }

        // Store merged attributes in node for use in close tag
        node.merged_attributes = mergedAttributes;

        // Indentation based on nesting level
        for (int i = 0; i < node.nesting_level; ++i)
        {
            svg_output_ << "  ";
        }

        closeOpenDefs();

        svg_output_ << "<g";

        // Add id attribute (sanitized)
        if (!node.resolved_identifier.empty())
        {
            svg_output_ << " id=\"" << escapeXmlAttribute(node.resolved_identifier) << "\"";
        }

        // The APS type is already conveyed by webcgm:type="..." on the same <g>
        // (see attribute_manager.cpp Tier 0 emission). The cgm-grobject /
        // cgm-layer / cgm-para / cgm-subpara / cgm-hotspot / cgm-grnode / cgm-aps
        // CSS classes were a duplicate styling hook; downstream consumers (the
        // GUI's SvgWebViewHelper, JS shim) detect by webcgm:type or by their own
        // attributes (data-aps-region-shape, data-layer, etc.), not by the class.
        // Class emission removed to reduce per-element overhead.

        std::map<std::string, std::string> metadataAttributes;
        std::string regionPolygonString;
        bool regionPolygonValid = false;

        if (!node.type.empty())
        {
            metadataAttributes["type"] = node.type;
        }

        // Add all APS attributes as data-* attributes (merged with XCF overrides)
        for (const auto &attr : mergedAttributes)
        {
            std::string attrValue = attr.second;

            if (attr.first == "region")
            {
                std::vector<CGMPoint> parsedPoints;
                const bool binaryRegion =
                    SDRParser::isBinarySDR(attr.second);

                if (binaryRegion)
                {
                    auto vdcCoordinates = SDRParser::parseRegionCoordinates(attr.second);
                    if (!vdcCoordinates.empty())
                    {
                        for (const auto &coordinate : vdcCoordinates)
                        {
                            double svg_x, svg_y;
                            transform_.transform(
                                coordinate.first,
                                coordinate.second,
                                svg_x,
                                svg_y);
                            parsedPoints.emplace_back(svg_x, svg_y);
                        }
                    }
                    else
                    {
                        // Failed to parse - skip this attribute
                        continue;
                    }
                }
                else
                {
                    if (auto points =
                            svg::ApsGeometry::parseRegionPoints(attrValue))
                    {
                        parsedPoints = std::move(*points);
                    }
                }

                if (!parsedPoints.empty())
                {
                    const auto geometry =
                        svg::ApsGeometry::normalizeRegion(parsedPoints);
                    if (binaryRegion || geometry.expanded_rectangle)
                    {
                        attrValue = geometry.metadata_value;
                    }

                    regionPolygonString = geometry.polygon_value;
                    regionPolygonValid = geometry.validPolygon();
                }
            }

            metadataAttributes[attr.first] = attrValue;
        }

        const auto serializationState =
            svg::ApsSerializer::analyze(
                node,
                mergedAttributes,
                std::move(metadataAttributes));
        if (serializationState.is_layer)
        {
            has_layer_aps_ = true;
        }
        if (serializationState.is_link)
        {
            has_linkuri_aps_ = true;
        }
        if (mergedAttributes.find("viewcontext") !=
            mergedAttributes.end())
        {
            has_viewcontext_aps_ = true;
        }

        // Propagate per-profile APS emission gates into the attribute manager so the
        // core data-aps-id / data-aps-name / data-aps-type emissions (and their S1000D
        // counterparts) are honoured too — the serialization map only carries
        // auxiliary keys, not the primary APS identifier triple.
        attribute_manager_.setApsEmissionGates(
            svg::ApsPolicy::emissionGates(conversion_plan_));
        auto attributeSet = attribute_manager_.transformAttributes(
            serializationState.aps_id,
            serializationState.aps_name,
            serializationState.serialization_attributes);
        svg_output_ <<
            attribute_manager_.generateSvgAttributes(attributeSet);

        std::set<std::string> emittedAttributeNames = {
            "id", "class", "style", "href", "xlink:href",
            "data-layer", "data-layer-title", "data-aps-visible"
        };
        const auto rememberAttributeNames = [&](const auto &attributes)
        {
            for (const auto &[name, value] : attributes)
            {
                (void)value;
                emittedAttributeNames.insert(name);
            }
        };
        rememberAttributeNames(attributeSet.webcgm);
        rememberAttributeNames(attributeSet.legacy);
        rememberAttributeNames(attributeSet.s1000d);
        rememberAttributeNames(attributeSet.s1000d_legacy);
        rememberAttributeNames(attributeSet.vendor);
        if (serializationState.is_link)
        {
            const auto customAttributes =
                svg::ApsPolicy::customAttributes(
                    conversion_plan_, node, aps_stack_,
                    emittedAttributeNames);
            for (const auto &[name, value] : customAttributes)
            {
                svg_output_ << " " << name << "=\""
                            << escapeXmlAttribute(value) << "\"";
            }
        }

        svg_output_ << svg::ApsSerializer::derivedGroupAttributes(
            serializationState,
            conversion_plan_);
        recordAPSMetadata(
            node,
            serializationState.metadata_attributes);

        const auto openMarkup = svg::ApsSerializer::bodyOpen(
            node,
            mergedAttributes,
            serializationState,
            conversion_plan_,
            regionPolygonString,
            regionPolygonValid);
        svg_output_ << openMarkup.markup;
        node.emitted_anchor_tag = openMarkup.emitted_anchor;
    }

    void SVGConverter::outputAPSCloseTag()
    {
        if (aps_stack_.empty())
            return;

        const APSNode &node = aps_stack_.back();
        svg_output_ << svg::ApsSerializer::close(
            node.nesting_level,
            node.emitted_anchor_tag);
    }

    void SVGConverter::updateClipPathDefinition()
    {
        if (!clip_enabled_ || !clip_rectangle_defined_)
        {
            clip_path_attribute_.clear();
            return;
        }

        double x1_svg = 0.0;
        double y1_svg = 0.0;
        double x2_svg = 0.0;
        double y2_svg = 0.0;
        transform_.transform(clip_rect_first_.x(), clip_rect_first_.y(), x1_svg, y1_svg);
        transform_.transform(clip_rect_second_.x(), clip_rect_second_.y(), x2_svg, y2_svg);

        double minX = std::min(x1_svg, x2_svg);
        double maxX = std::max(x1_svg, x2_svg);
        double minY = std::min(y1_svg, y2_svg);
        double maxY = std::max(y1_svg, y2_svg);
        double width = maxX - minX;
        double height = maxY - minY;

        if (width <= 0.0 || height <= 0.0)
        {
            clip_path_attribute_.clear();
            return;
        }

        std::ostringstream keyStream;
        keyStream.setf(std::ios::fixed);
        keyStream << std::setprecision(6)
                  << minX << "," << minY << "," << width << "," << height;
        std::string key = keyStream.str();

        if (auto it = clip_path_cache_.find(key); it != clip_path_cache_.end())
        {
            clip_path_attribute_ = " clip-path=\"url(#" + it->second + ")\" ";
            return;
        }

        if (!defs_open_)
        {
            svg_output_ << "  <defs>\n";
            defs_open_ = true;
        }

        std::string clipId = "clipPath" + std::to_string(++clip_path_counter_);
        // WebCGM 2.1 §3.2.3 restricts clipping to axis-aligned rectangles; emit SVG clipPath accordingly.
        svg_output_ << "    <clipPath id=\"" << clipId << "\" clipPathUnits=\"userSpaceOnUse\">\n";
        svg_output_ << "      <rect x=\"" << minX << "\" y=\"" << minY
                    << "\" width=\"" << width << "\" height=\"" << height << "\" />\n";
        svg_output_ << "    </clipPath>\n";

        clip_path_cache_[key] = clipId;
        clip_path_attribute_ = " clip-path=\"url(#" + clipId + ")\" ";
    }

    std::string SVGConverter::clipPathAttribute() const
    {
        return clip_path_attribute_;
    }

    void SVGConverter::closeOpenDefs()
    {
        if (defs_open_)
        {
            svg_output_ << "  </defs>\n";
            defs_open_ = false;
        }
    }

    std::string SVGConverter::debugCommandAttribute() const
    {
        if (!debug_fill_logging_)
        {
            return {};
        }
        std::ostringstream ss;
        ss << " data-cgm-cmd=\"" << current_command_index_ << "\" ";
        return ss.str();
    }

    double SVGConverter::currentTextRotationDegrees() const
    {
        // WebCGM 2.1 §3.4.3 defines text orientation via baseline (xUp) and up-vector (yUp).
        double origin_x = 0.0;
        double origin_y = 0.0;
        transform_.transform(0.0, 0.0, origin_x, origin_y);

        double base_x = 0.0;
        double base_y = 0.0;
        transform_.transform(character_orientation_base_.x(), character_orientation_base_.y(), base_x, base_y);

        double vec_x = base_x - origin_x;
        double vec_y = base_y - origin_y;
        double magnitude = std::hypot(vec_x, vec_y);
        if (magnitude < 1e-9)
        {
            return 0.0;
        }

        double angle = std::atan2(vec_y, vec_x) * 180.0 / M_PI;
        return angle;
    }

} // namespace opencgm


