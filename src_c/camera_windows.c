#include "_camera.h"
#include "pgcompat.h"

#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <combaseapi.h>
#include <mftransform.h>

#define RELEASE(obj) if (obj) {obj->lpVtbl->Release(obj);}
#define CHECKHR(hr) if FAILED(hr) {PyErr_Format(pgExc_SDLError, "Media Foundation HRESULT failure %i on line %i", hr, __LINE__); return 0;}

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
        hr = reader->lpVtbl->GetNativeMediaType(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, type_count, &media_type);
        if (hr) {
            break;
        }
        type_count++;
        RELEASE(media_type);
    }

    IMFMediaType** native_types = malloc(sizeof(IMFMediaType*) * type_count);
    int* diagonal_distances = malloc(sizeof(int) * type_count);

    GUID q1;

    for (int i=0; i < type_count; i++) {
        hr = reader->lpVtbl->GetNativeMediaType(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &media_type);

        media_type->lpVtbl->GetUINT64(media_type, &MF_MT_FRAME_SIZE, &size);
        printf("width=%lli, height=%lli\n", size >> 32, size << 32 >> 32);

        media_type->lpVtbl->GetGUID(media_type, &MF_MT_SUBTYPE, &q1);
        printf("subtype=%li\n", q1.Data1);

        t_width = size >> 32;
        t_height = size << 32 >> 32;

        native_types[i] = media_type;
        diagonal_distances[i] = (int)pow(t_width, 2) + (int)pow(t_height, 2);
    }

    int difference = 100000000;
    int current_difference;
    int index = 0;

    for (int i=0; i < type_count; i++) {
        current_difference = diagonal_distances[i] - ((int)pow(width, 2) + (int)pow(height, 2));
        current_difference = abs(current_difference);
        if (current_difference < difference) {
            index = i;
            difference = current_difference;
        }
    }

    printf("chosen index # =%i\n", index);

    hr = MFCreateMediaType(&media_type);
    CHECKHR(hr);

    hr = media_type->lpVtbl->SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    CHECKHR(hr);
    hr = media_type->lpVtbl->SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    CHECKHR(hr);


    hr = native_types[index]->lpVtbl->GetUINT64(native_types[index], &MF_MT_FRAME_SIZE, &size);
    CHECKHR(hr);

    //aslfjs
    //hr = native_types[index]->lpVtbl->SetGUID(native_types[index], &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    //CHECKHR(hr);
    //*mp = native_types[index];
    //return 1;

    //RELEASE(native_types[index]);

    printf("size=%lli\n", size);
    printf("width=%lli, height=%lli\n", size >> 32, size << 32 >> 32);

    //hr = media_type->lpVtbl->SetUINT64(media_type, &MF_MT_FRAME_SIZE, size);
    //CHECKHR(hr);

    //hr = media_type->lpVtbl->SetUINT32(media_type, &MF_MT_DEFAULT_STRIDE, (size >> 32) * 4);
    //CHECKHR(hr);
    //hr = media_type->lpVtbl->SetUINT32(media_type, &MF_MT_SAMPLE_SIZE, (size >> 32) * (size << 32 >> 32) * 4);
    //CHECKHR(hr);

    //*mp = media_type;

    *mp = native_types[index];
    return 1;
}

int windows_open_device(pgCameraObject *self) {
    IMFMediaSource *source;
    IMFSourceReader *reader = NULL;
    IMFMediaType *media_type = NULL;
    HRESULT hr;

    /* setup the stuff before MFCreateSourceReaderFromMediaSource is called */
    hr = CoInitializeEx(0, COINIT_MULTITHREADED); //I don't want multithreading, but it seems default
    CHECKHR(hr);
    hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    CHECKHR(hr);

    // Another attempt to open webcam in another resolution
    //UINT64 s = 5497558139600;
    //hr = self->activate->lpVtbl->SetUINT64(self->activate, &MF_MT_FRAME_SIZE, s);
    //CHECKHR(hr);

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

    hr = conv_type->lpVtbl->SetUINT64(conv_type, &MF_MT_FRAME_SIZE, size);
    CHECKHR(hr);

    hr = reader->lpVtbl->SetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, media_type);
    CHECKHR(hr);

    //RELEASE(media_type);

    //hr = reader->lpVtbl->GetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, &media_type);
    //CHECKHR(hr);
    
    // Testing out a way to get webcam sizes
    // Upper 32 bits / lower 32 bits signify width and height
    // See https://docs.microsoft.com/en-us/windows/win32/medfound/mf-mt-frame-size-attribute
    /*
    UINT64 size;
    media_type->lpVtbl->GetUINT64(media_type, &MF_MT_FRAME_SIZE, &size);
    printf("width=%li\n", size >> 32);
    printf("height=%li\n", size << 32 >> 32);
    */
    
    IMFVideoProcessorControl* control;
    IMFTransform* transform;
    hr = CoCreateInstance(&CLSID_VideoProcessorMFT, NULL, CLSCTX_INPROC_SERVER, &IID_IMFVideoProcessorControl, &control);
    CHECKHR(hr);

    hr = CoCreateInstance(&CLSID_VideoProcessorMFT, NULL, CLSCTX_INPROC_SERVER, &IID_IMFTransform, &transform);
    CHECKHR(hr);

    hr = control->lpVtbl->SetMirror(control, MIRROR_VERTICAL | MIRROR_HORIZONTAL);
    CHECKHR(hr);

    self->control = control;
    self->transform = transform;

    hr = transform->lpVtbl->SetInputType(transform, 0, media_type, 0);
    CHECKHR(hr);

    hr = transform->lpVtbl->SetOutputType(transform, 0, conv_type, 0);
    CHECKHR(hr);

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
    DWORD pdwStreamFlags;

    //clock_t c = clock();
    hr = reader->lpVtbl->ReadSample(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, 0, &pdwStreamFlags, NULL, &sample);
    //c -= clock();
    //printf("time elapsed = %f\n", c / (double)CLOCKS_PER_SEC);

    if (hr == -1072875772) { //MF_E_HW_MFT_FAILED_START_STREAMING
        PyErr_SetString(PyExc_SystemError, "Camera already in use");
        return 0;
    }

    CHECKHR(hr);

    if (sample) {
        SDL_LockSurface(surf);

        //MFT_INPUT_STREAM_INFO ininfo;
        //hr = self->transform->lpVtbl->GetInputStreamInfo(self->transform, 0, &ininfo);
        //CHECKHR(hr);
        //printf("INPUT BYTES SIZE=%i\n", ininfo.cbSize);        

        hr = self->transform->lpVtbl->ProcessInput(self->transform, 0, sample, 0);
        CHECKHR(hr);

        MFT_OUTPUT_DATA_BUFFER mft_buffer[1];

        MFT_OUTPUT_DATA_BUFFER x;

        MFT_OUTPUT_STREAM_INFO info;
        hr = self->transform->lpVtbl->GetOutputStreamInfo(self->transform, 0, &info);
        CHECKHR(hr);
        //printf("OUTPUT BYTES SIZE=%i\n", info.cbSize);

        IMFSample* ns;
        hr = MFCreateSample(&ns);
        CHECKHR(hr);

        IMFMediaBuffer* bs;
        CHECKHR(MFCreateMemoryBuffer(info.cbSize, &bs));
        CHECKHR(ns->lpVtbl->AddBuffer(ns, bs));

        //IMF2DBuffer* b2;
        //bs->lpVtbl->QueryInterface(bs, &IID_IMF2DBuffer, &b2);
        //CHECKHR(ns->lpVtbl->AddBuffer(ns, b2));

        x.pSample = ns;
        x.dwStreamID = 0;
        x.dwStatus = 0;
        x.pEvents = NULL;
        mft_buffer[0] = x;

        DWORD out;
        hr = self->transform->lpVtbl->ProcessOutput(self->transform, 0, 1, mft_buffer, &out);
        CHECKHR(hr);

        BYTE *buf_data;
        DWORD buf_max_length;
        DWORD buf_length;
        bs->lpVtbl->Lock(bs, &buf_data, &buf_max_length, &buf_length);

        // optimized for 32 bit output surfaces
        // this won't be possible always, TODO implement switching logic
        memcpy(surf->pixels, buf_data, buf_length);

        bs->lpVtbl->Unlock(bs);

        RELEASE(bs);
        RELEASE(sample);

        SDL_UnlockSurface(surf);
    }

    return 1;
}