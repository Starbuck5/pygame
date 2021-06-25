#include "_camera.h"
#include "pgcompat.h"

#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <combaseapi.h>

#define RELEASE(obj) if (obj) {obj->lpVtbl->Release(obj);}
#define CHECKHR(hr) if FAILED(hr) {PyErr_SetString(pgExc_SDLError, "Media Foundation HRESULT failure"); return NULL;}

//__LINE__

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

    /*
    if (res) {
        delete [] res;
    }
    */
    //return hr;
    return (WCHAR *)res;
}

WCHAR **
windows_list_cameras(int *num_devices) {
    WCHAR** devices = NULL;
    IMFMediaSource *pSource = NULL;
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

    for (int i=0; i<count; i++) {
        devices[i] = get_attr_string(ppDevices[i]);
    }

    // TODO: Release ppDevices?

    *num_devices = count;
    return devices;
}

//wcscmp
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

    for (int i=0; i<count; i++) {
        _device_name = get_attr_string(ppDevices[i]);
        if (!wcscmp(_device_name, device_name)) {
            free(_device_name);
            return ppDevices[i];
        }
        free(_device_name);
    }

    // TODO: Release ppDevices?

    return NULL;
}

int windows_open_device(pgCameraObject *self) {
    /* setup the stuff before MFCreateSourceReaderFromMediaSource is called */
    // TODO: error check
    CoInitializeEx(0, COINIT_MULTITHREADED); //I don't want multithreading, but it seems default
    MFStartup(MF_VERSION, MFSTARTUP_LITE);

    IMFMediaSource *source;
    IMFSourceReader *reader = NULL;
    IMFMediaType *media_type = NULL;
    HRESULT hr = NULL;

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
    
    hr = MFCreateMediaType(&media_type);
    CHECKHR(hr);

    hr = media_type->lpVtbl->SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    CHECKHR(hr);
    hr = media_type->lpVtbl->SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    CHECKHR(hr);

    hr = reader->lpVtbl->SetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, media_type);
    CHECKHR(hr);

    RELEASE(media_type);

    hr = reader->lpVtbl->GetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, &media_type);
    CHECKHR(hr);


    // Testing out a way to get webcam sizes
    // Upper 32 bits / lower 32 bits signify width and height
    // See https://docs.microsoft.com/en-us/windows/win32/medfound/mf-mt-frame-size-attribute
    UINT64 size;
    media_type->lpVtbl->GetUINT64(media_type, &MF_MT_FRAME_SIZE, &size);
    printf("size=%lli\n", size);

    return 1;
}

int windows_close_device(pgCameraObject *self) {
    RELEASE(self->reader);
    return 1;
}

int windows_read_frame(pgCameraObject *self, SDL_Surface *surf) {
    IMFSourceReader *reader = self->reader;
    IMFSample *sample = NULL;

    HRESULT hr;

    LONGLONG pllTimestamp;
    DWORD pdwStreamFlags;

    //printf("I'm a frame, what? \n");

    hr = reader->lpVtbl->ReadSample(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, 0, &pdwStreamFlags, &pllTimestamp, &sample);
    CHECKHR(hr);

    if (sample) {
        SDL_LockSurface(surf);

        //use IMF 2d buffer instead?
        IMFMediaBuffer *buf = NULL;
        sample->lpVtbl->GetBufferByIndex(sample, 0, &buf);
        //printf("buf=%p\n", buf);

        BYTE *buf_data;
        DWORD buf_max_length;
        DWORD buf_length;
        buf->lpVtbl->Lock(buf, &buf_data, &buf_max_length, &buf_length);
        //printf("buf_data=%p\n", buf_data);
        //printf("buf_length=%i\n", buf_length);
        //printf("buf_data=\n%s\n", buf_data);

        printf("first pixel = (%i, %i, %i, %i)\n", buf_data[0], buf_data[1], buf_data[2], buf_data[3]);

        rgb24_to_rgb(buf_data, surf->pixels, buf_length / 4, surf->format);

        //Media Foundation is giving RGBA, I think

        //SDL 24 bit surfaces use BGR, hopefully
        Uint8* d8 = (Uint8 *)surf->pixels;
        Uint8* s = (Uint8 *)buf_data;
        for (int i = 0; i < buf_length / 4; i++) {
            *d8++ = *s++;
            *d8++ = *s++;
            *d8++ = *s++;
            *s++;
        }

        /*
        Uint32 *src = (Uint32*)buf_data;
        Uint32 *dst = (Uint32*)surf->pixels;
        Uint8 r, g, b;
        int rshift, gshift, bshift, rloss, gloss, bloss;

        rshift = surf->format->Rshift;
        gshift = surf->format->Gshift;
        bshift = surf->format->Bshift;
        rloss = surf->format->Rloss;
        gloss = surf->format->Gloss;
        bloss = surf->format->Bloss;

        for (int i=0; i<buf_length; i++) {
            //px = (Uint8*)buf_data[i];
            printf("buf_data[i] = %i\n", buf_data[i]);
            r = buf_data[i] & 0xFF;
            g = (buf_data[i] >> 8) & 0xFF;
            b = (buf_data[i] >> 16) & 0xFF;
            *dst++ = ((r >> rloss) << rshift) | ((g >> gloss) << gshift) |
                     ((b >> bloss) << bshift);
        }
        */

        buf->lpVtbl->Unlock(buf);

        RELEASE(buf);
        RELEASE(sample);

        SDL_UnlockSurface(surf);
    }

    return 1;
}