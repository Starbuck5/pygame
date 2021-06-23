#include "_camera.h"
#include "pgcompat.h"

#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <combaseapi.h>

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
    IMFMediaType *media_type;

    self->activate->lpVtbl->ActivateObject(self->activate, &IID_IMFMediaSource, &source);
    self->source = source;

    MFCreateSourceReaderFromMediaSource(source, NULL, &reader);

    self->reader = reader;
    
    //TODO: release the interface
    MFCreateMediaType(&media_type);

    media_type->lpVtbl->SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    media_type->lpVtbl->SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB8);

    reader->lpVtbl->SetCurrentMediaType(reader, 0, NULL, media_type);

    return 1;
}

int windows_read_frame(pgCameraObject *self, SDL_Surface *surf) {
    printf("I'm a frame, what? \n");

    IMFSourceReader *reader = self->reader;
    IMFSample *sample = NULL;

    HRESULT hr;

    LONGLONG pllTimestamp;
    DWORD pdwStreamFlags;

    hr = reader->lpVtbl->ReadSample(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, 0, &pdwStreamFlags, &pllTimestamp, &sample);

    printf("sample=%p\n", sample);
    printf("hr=%i\n", hr);
    //printf("MF_E_INVALIDREQUEST=%i\n", MF_E_INVALIDREQUEST);

    return 1;
}