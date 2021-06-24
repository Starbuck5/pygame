#include "_camera.h"
#include "pgcompat.h"

#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <combaseapi.h>

#define RELEASE(obj) obj->lpVtbl->Release(obj)

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

int windows_init_device(pgCameraObject *self) {
    printf("imagine that\n");
    return 1;
}

int windows_open_device(pgCameraObject *self) {
    printf("made it here\n");

    /* setup the stuff before MFCreateSourceReaderFromMediaSource is called */
    // TODO: error check
    CoInitializeEx(0, COINIT_MULTITHREADED); //I don't want multithreading, but it seems default
    MFStartup(MF_VERSION, MFSTARTUP_LITE);

    IMFMediaSource *source;
    IMFSourceReader *reader = NULL;
    IMFMediaType *media_type = NULL;

    self->activate->lpVtbl->ActivateObject(self->activate, &IID_IMFMediaSource, &source);
    self->source = source;

    MFCreateSourceReaderFromMediaSource(source, NULL, &reader);

    self->reader = reader;
    
    HRESULT hr;

    //TODO: release the interface
    hr = MFCreateMediaType(&media_type);
    printf("RESULT CREATEMEDIA=%i\n", hr);

    hr = media_type->lpVtbl->SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    printf("RESULT SETMAJOR=%i\n", hr);
    hr = media_type->lpVtbl->SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB24);
    printf("RESULT SETMINOR=%i\n", hr);

    hr = reader->lpVtbl->SetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, media_type);
    printf("RESULT SETMEDIA=%i\n", hr);

    /* aborted attempt to find native media type */
    //reader->lpVtbl->GetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &media_type);
    //GUID g;
    //media_type->lpVtbl->GetGUID(media_type, &MF_MT_SUBTYPE, &g);
    //printf

    return 1;
}

int windows_read_frame(pgCameraObject *self, SDL_Surface *surf) {
    IMFSourceReader *reader = self->reader;
    IMFSample *sample = NULL;

    HRESULT hr;

    LONGLONG pllTimestamp;
    DWORD pdwStreamFlags;

    printf("I'm a frame, what? \n");

    hr = reader->lpVtbl->ReadSample(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, 0, &pdwStreamFlags, &pllTimestamp, &sample);

    printf("sample=%p\n", sample);
    printf("hr=%i\n", hr);
    //printf("MF_E_INVALIDREQUEST=%i\n", MF_E_INVALIDREQUEST);

    if (sample) {
        SDL_LockSurface(surf);
        printf("made it 1\n");

        //use IMF 2d buffer instead?
        IMFMediaBuffer *buf = NULL;
        sample->lpVtbl->GetBufferByIndex(sample, 0, &buf);
        printf("buf=%p\n", buf);
        printf("made it 2\n");

        //DWORD buf_length;
        //buf->lpVtbl->GetCurrentLength(buf, &buf_length);
        //printf("made it 3\n");

        //DWORD buf_count;
        //sample->lpVtbl->GetBufferCount(sample, &buf_count);
        //printf("buf_count=%i\n", buf_count);

        BYTE *buf_data;
        DWORD buf_max_length;
        DWORD buf_length;
        buf->lpVtbl->Lock(buf, &buf_data, &buf_max_length, &buf_length);
        printf("buf_data=%p\n", buf_data);
        printf("buf_length=%i\n", buf_length);
        //printf("buf_data=\n%s\n", buf_data);
        printf("made it 3\n");

        rgb24_to_rgb(buf_data, surf->pixels, buf_length / 3, surf->format);
        printf("made it 4\n");

        buf->lpVtbl->Unlock(buf);

        RELEASE(buf);
        RELEASE(sample);

        SDL_UnlockSurface(surf);
    }



    return 1;
}