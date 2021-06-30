/*
 * Windows Camera - webcam support for pygame
 * Original Author: Charlie Hayden, 2021
 *
 * This sub-module adds native support for windows webcams to pygame,
 * made possible by Microsoft Media Foundation.
 */

#include "_camera.h"
#include "pgcompat.h"

// these are already included in camera.h, but having them here
// makes all the types be recognized by VS code
#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <combaseapi.h>
#include <mftransform.h>

#define RELEASE(obj) if (obj) {obj->lpVtbl->Release(obj);}

// HRESULT failure numbers can be looked up on hresult.info to get the actual name
#define CHECKHR(hr) if FAILED(hr) {PyErr_Format(pgExc_SDLError, "Media Foundation HRESULT failure %i on line %i", hr, __LINE__); return 0;}

#define FIRST_VIDEO MF_SOURCE_READER_FIRST_VIDEO_STREAM

// drawn from:
//https://docs.microsoft.com/en-us/windows/win32/medfound/video-subtype-guids
#define NUMRGB 8
static GUID* rgb_types[NUMRGB] = {&MFVideoFormat_RGB8, &MFVideoFormat_RGB555,
                                  &MFVideoFormat_RGB565, &MFVideoFormat_RGB24,
                                  &MFVideoFormat_RGB32, &MFVideoFormat_ARGB32,
                                  &MFVideoFormat_A2R10G10B10,
                                  &MFVideoFormat_A16B16G16R16F};

#define NUMYUV 25
static GUID* yuv_types[NUMYUV] = {&MFVideoFormat_AI44, &MFVideoFormat_AYUV,
                                  &MFVideoFormat_I420, &MFVideoFormat_IYUV,
                                  &MFVideoFormat_NV11, &MFVideoFormat_NV12,
                                  &MFVideoFormat_UYVY, &MFVideoFormat_Y41P,
                                  &MFVideoFormat_Y41T, &MFVideoFormat_Y42T,
                                  &MFVideoFormat_YUY2, &MFVideoFormat_YVU9,
                                  &MFVideoFormat_YV12, &MFVideoFormat_YVYU,
                                  &MFVideoFormat_P010, &MFVideoFormat_P016,
                                  &MFVideoFormat_P210, &MFVideoFormat_P216,
                                  &MFVideoFormat_v210, &MFVideoFormat_v216,
                                  &MFVideoFormat_v410, &MFVideoFormat_Y210,
                                  &MFVideoFormat_Y216, &MFVideoFormat_Y410,
                                  &MFVideoFormat_Y416};

int _format_is_rgb(GUID format) {
    for (int i=0; i<NUMRGB; i++) {
        if (format.Data1 == rgb_types[i]->Data1)
            return 1;
    }
    return 0;
}

int _format_is_yuv(GUID format) {
    for (int i=0; i<NUMYUV; i++) {
        if (format.Data1 == yuv_types[i]->Data1)
            return 1;
    }
    return 0;
}

WCHAR *
get_attr_string(IMFActivate *pActive) {
    HRESULT hr = S_OK;
    UINT32 cchLength = 0;
    WCHAR *res = NULL;

    hr = pActive->lpVtbl->GetStringLength(pActive, &MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &cchLength);
    
    if (SUCCEEDED(hr)) {
        res = malloc(sizeof(WCHAR)*(cchLength+1));
        if (res == NULL)
            hr = E_OUTOFMEMORY;
    }

    if (SUCCEEDED(hr)) {
        hr = pActive->lpVtbl->GetString(pActive,
            &MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, res, cchLength + 1, &cchLength);
    }

    return (WCHAR *)res;
}

WCHAR **
windows_list_cameras(int *num_devices) {
    WCHAR** devices = NULL;
    IMFAttributes *pAttributes = NULL;
    IMFActivate **ppDevices = NULL;

    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) {
        printf("oof\n");
    }

    hr = pAttributes->lpVtbl->SetGUID(pAttributes, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 
                              &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    if (FAILED(hr)) {
        printf("oof2\n");
    }

    UINT32 count = -1;
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);

    if (FAILED(hr)) {
        printf("oof_3\n");
    }

    devices = (WCHAR **)malloc(sizeof(WCHAR *) * count);

    for (int i=0; i<(int)count; i++) {
        devices[i] = get_attr_string(ppDevices[i]);
    }

    for (int i=0; i<(int)count; i++) {
        RELEASE(ppDevices[i]);  
    }
    RELEASE(pAttributes)
    *num_devices = count;
    return devices;
}

IMFActivate *
windows_device_from_name(WCHAR* device_name) {
    IMFAttributes *pAttributes = NULL;
    IMFActivate **ppDevices = NULL;
    WCHAR* _device_name = NULL;

    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) {
        printf("oof\n");
    }

    hr = pAttributes->lpVtbl->SetGUID(pAttributes, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 
                              &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    if (FAILED(hr)) {
        printf("oof2\n");
    }

    UINT32 count = -1;
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);

    if (FAILED(hr)) {
        printf("oof_3\n");
    }

    for (int i=0; i<(int)count; i++) {
        _device_name = get_attr_string(ppDevices[i]);
        if (!wcscmp(_device_name, device_name)) {
            free(_device_name);
            return ppDevices[i];
        }
        free(_device_name);
    }

    for (int i=0; i<(int)count; i++) {
        RELEASE(ppDevices[i]);  
    }
    RELEASE(pAttributes);
    return NULL;
}

int _create_media_type(IMFMediaType** mp, IMFSourceReader* reader, int width, int height) {
    HRESULT hr;
    IMFMediaType* media_type = NULL;
    UINT64 size;
    int type_count = 0;
    UINT32 t_width, t_height;

    /* Find out how many native media types there are (different resolution modes, mostly) */
    while(1) {
        hr = reader->lpVtbl->GetNativeMediaType(reader, FIRST_VIDEO, type_count, &media_type);
        if (hr) {
            break;
        }
        type_count++;
        RELEASE(media_type);
    }

    IMFMediaType** native_types = malloc(sizeof(IMFMediaType*) * type_count);
    int* diagonal_distances = malloc(sizeof(int) * type_count);

    GUID subtype;

    // This is interesting
    // https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/9d6a8704-764f-46df-a41c-8e9d84f7f0f3/mjpg-encoded-media-type-is-not-available-for-usbuvc-webcameras-after-windows-10-version-1607-os
    // Compressed video modes listed below are "backwards compatibility modes"
    // And will always have an uncompressed counterpart (NV12 / YUY2)
    // At least in Windows 10 since 2016

    for (int i=0; i < type_count; i++) {
        hr = reader->lpVtbl->GetNativeMediaType(reader, FIRST_VIDEO, i, &media_type);
        CHECKHR(hr);

        hr = media_type->lpVtbl->GetGUID(media_type, &MF_MT_SUBTYPE, &subtype);
        CHECKHR(hr);

        if (_format_is_rgb(subtype) || _format_is_yuv(subtype)) {
            hr = media_type->lpVtbl->GetUINT64(media_type, &MF_MT_FRAME_SIZE, &size);
            CHECKHR(hr);
    
            t_width = size >> 32;
            t_height = size << 32 >> 32;

            native_types[i] = media_type;
            diagonal_distances[i] = (int)pow(t_width, 2) + (int)pow(t_height, 2);
        }
        else {
            native_types[i] = NULL;
            diagonal_distances[i] = 0;
        }
    }

    int difference = 100000000;
    int current_difference;
    int index = 0;

    for (int i=0; i < type_count; i++) {
        current_difference = diagonal_distances[i] - ((int)pow(width, 2) + (int)pow(height, 2));
        current_difference = abs(current_difference);
        if (current_difference < difference && native_types[index]) {
            index = i;
            difference = current_difference;
        }
    }

    //TODO: what happens if it doesn't select anything?

    printf("chosen index # =%i\n", index);

    hr = native_types[index]->lpVtbl->GetUINT64(native_types[index], &MF_MT_FRAME_SIZE, &size);
    CHECKHR(hr);

    printf("chosen width=%lli, chosen height=%lli\n", size >> 32, size << 32 >> 32);

    // Can't hurt to tell the webcam to use its highest possible framerate
    // Although I haven't seen any upside from this either
    UINT64 fps_max_ratio;
    hr = native_types[index]->lpVtbl->GetUINT64(native_types[index], &MF_MT_FRAME_RATE_RANGE_MAX, &fps_max_ratio);
    CHECKHR(hr);
    hr = native_types[index]->lpVtbl->SetUINT64(native_types[index], &MF_MT_FRAME_RATE, fps_max_ratio);
    CHECKHR(hr);

    *mp = native_types[index];
    return 1;
}

DWORD WINAPI update_function(LPVOID lpParam) {
    pgCameraObject* self = (pgCameraObject*) lpParam;

    IMFSample *sample;
    HRESULT hr;
    DWORD pdwStreamFlags;

    IMFSourceReader* reader = self->reader;
    
    while(1) {
        sample = NULL;

        hr = reader->lpVtbl->ReadSample(reader, FIRST_VIDEO, 0, 0, &pdwStreamFlags, NULL, &sample);
        if (hr == -1072875772) { //MF_E_HW_MFT_FAILED_START_STREAMING
            PyErr_SetString(PyExc_SystemError, "Camera already in use");
            return 0;
            //TODO: how are errors from this thread going to work?
        }
        CHECKHR(hr);

        if (!self->open) {
            RELEASE(sample);
            break;
        }

        if (sample) {
            hr = self->transform->lpVtbl->ProcessInput(self->transform, 0, sample, 0);
            CHECKHR(hr);

            MFT_OUTPUT_DATA_BUFFER mft_buffer[1];
            MFT_OUTPUT_DATA_BUFFER x;

            IMFSample* ns;
            hr = MFCreateSample(&ns);
            CHECKHR(hr);

            CHECKHR(ns->lpVtbl->AddBuffer(ns, self->buf));

            x.pSample = ns;
            x.dwStreamID = 0;
            x.dwStatus = 0;
            x.pEvents = NULL;
            mft_buffer[0] = x;

            DWORD out;
            hr = self->transform->lpVtbl->ProcessOutput(self->transform, 0, 1, mft_buffer, &out);
            CHECKHR(hr);

            self->buffer_ready = 1;
        }

        RELEASE(sample);
    }

    printf("exiting 2nd thread...\n");
    ExitThread(0);
}


int windows_open_device(pgCameraObject *self) {
    IMFMediaSource *source;
    IMFSourceReader *reader = NULL;
    IMFMediaType *media_type = NULL;
    HRESULT hr;

    /* setup the stuff before MFCreateSourceReaderFromMediaSource is called */
    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    CHECKHR(hr);
    hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    CHECKHR(hr);

    hr = self->activate->lpVtbl->ActivateObject(self->activate, &IID_IMFMediaSource, &source);
    CHECKHR(hr);
    self->source = source;

    // Set the source reader to use video processing
    // This allows it to convert any media stream type to RGB32
    // It is not recommended for realtime use, but it seems to work alright
    IMFAttributes *rsa;
    MFCreateAttributes(&rsa, 1);
    rsa->lpVtbl->SetUINT32(rsa, &MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, 1);

    hr = MFCreateSourceReaderFromMediaSource(source, rsa, &reader);
    RELEASE(rsa);
    CHECKHR(hr);

    self->reader = reader;
    
    if(!_create_media_type(&media_type, reader, self->width, self->height)) {
        return 0;
    }

    UINT64 size;
    hr = media_type->lpVtbl->GetUINT64(media_type, &MF_MT_FRAME_SIZE, &size);
    CHECKHR(hr);

    self->width = size >> 32;
    self->height = size << 32 >> 32;

    IMFMediaType* conv_type;
    CHECKHR(MFCreateMediaType(&conv_type));
    hr = conv_type->lpVtbl->SetGUID(conv_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    CHECKHR(hr);
    hr = conv_type->lpVtbl->SetGUID(conv_type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    CHECKHR(hr);

    // make sure the output is right side up by default
    hr = conv_type->lpVtbl->SetUINT32(conv_type, &MF_MT_DEFAULT_STRIDE, self->width * 4);
    CHECKHR(hr);

    hr = conv_type->lpVtbl->SetUINT64(conv_type, &MF_MT_FRAME_SIZE, size);
    CHECKHR(hr);

    hr = reader->lpVtbl->SetCurrentMediaType(reader, FIRST_VIDEO, NULL, media_type);
    CHECKHR(hr);
    
    IMFVideoProcessorControl* control;
    IMFTransform* transform;

    hr = CoCreateInstance(&CLSID_VideoProcessorMFT, NULL, CLSCTX_INPROC_SERVER, &IID_IMFTransform, &transform);
    CHECKHR(hr);

    hr = transform->lpVtbl->QueryInterface(transform, &IID_IMFVideoProcessorControl, &control);
    CHECKHR(hr);

    self->control = control;
    self->transform = transform;

    hr = transform->lpVtbl->SetInputType(transform, 0, media_type, 0);
    CHECKHR(hr);

    hr = transform->lpVtbl->SetOutputType(transform, 0, conv_type, 0);
    CHECKHR(hr);

    //hr = control->lpVtbl->SetMirror(control, MIRROR_VERTICAL | MIRROR_HORIZONTAL);
    //CHECKHR(hr);

    MFT_OUTPUT_STREAM_INFO info;
    hr = self->transform->lpVtbl->GetOutputStreamInfo(self->transform, 0, &info);
    CHECKHR(hr);
    //printf("OUTPUT BYTES SIZE=%i\n", info.cbSize);

    CHECKHR(MFCreateMemoryBuffer(info.cbSize, &self->buf));

    HANDLE update_thread = CreateThread(NULL, 0, update_function, self, 0, NULL);

    self->t_handle = update_thread;

    return 1;
}

int windows_close_device(pgCameraObject *self) {
    self->open = 0;
    WaitForSingleObject(self->t_handle, 3000);

    RELEASE(self->reader);
    CHECKHR(MFShutdown());
    return 1;
}

int windows_read_frame(pgCameraObject *self, SDL_Surface *surf) {
    if (self->buf) {
        BYTE *buf_data;
        DWORD buf_max_length;
        DWORD buf_length;
        self->buf->lpVtbl->Lock(self->buf, &buf_data, &buf_max_length, &buf_length);

        SDL_LockSurface(surf);

        // optimized for 32 bit output surfaces
        // this won't be possible always, TODO implement switching logic
        memcpy(surf->pixels, buf_data, buf_length);

        SDL_UnlockSurface(surf);

        self->buf->lpVtbl->Unlock(self->buf);

        self->buffer_ready = 0;
    }

    return 1;
}

int windows_frame_ready(pgCameraObject *self) {
    return self->buffer_ready;
}