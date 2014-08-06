//
// Copyright (c) 2008 Advanced Micro Devices, Inc. All rights reserved.
//

#ifndef CL_COMMON_HPP_
#define CL_COMMON_HPP_

#include "top.hpp"
#include "platform/runtime.hpp"
#include "platform/command.hpp"
#include "platform/memory.hpp"
#include "platform/video_session.hpp"
#include "thread/thread.hpp"

#include <vector>
#include <utility>

//! \cond ignore
namespace amd {

template <typename T>
class NotNullWrapper
{
private:
    T* const ptrOrNull_;

protected:
    explicit NotNullWrapper(T* ptrOrNull)
        : ptrOrNull_(ptrOrNull)
    { }

public:
    void operator = (T value) const
    {
        if (ptrOrNull_ != NULL) {
            *ptrOrNull_ = value;
        }
    }
};

template <typename T>
class NotNullReference : protected NotNullWrapper<T>
{
public:
    explicit NotNullReference(T* ptrOrNull)
        : NotNullWrapper<T>(ptrOrNull)
    { }

    const NotNullWrapper<T>& operator * () const { return *this; }
};

} // namespace amd

template <typename T>
inline amd::NotNullReference<T>
not_null(T* ptrOrNull)
{
    return amd::NotNullReference<T>(ptrOrNull);
}

#define CL_CHECK_THREAD(thread)                                              \
    (thread != NULL || ((thread = new amd::HostThread()) != NULL             \
            && thread == amd::Thread::current()))

#define RUNTIME_ENTRY_RET(ret, func, args)                                   \
CL_API_ENTRY ret CL_API_CALL                                                 \
func args                                                                    \
{                                                                            \
    amd::Thread* thread = amd::Thread::current();                            \
    if (!CL_CHECK_THREAD(thread)) {                                          \
        *not_null(errcode_ret) = CL_OUT_OF_HOST_MEMORY;                      \
        return (ret) 0;                                                      \
    }

#define RUNTIME_ENTRY_RET_NOERRCODE(ret, func, args)                         \
CL_API_ENTRY ret CL_API_CALL                                                 \
func args                                                                    \
{                                                                            \
    amd::Thread* thread = amd::Thread::current();                            \
    if (!CL_CHECK_THREAD(thread)) {                                          \
        return (ret) 0;                                                      \
    }

#define RUNTIME_ENTRY(ret, func, args)                                       \
CL_API_ENTRY ret CL_API_CALL                                                 \
func args                                                                    \
{                                                                            \
    amd::Thread* thread = amd::Thread::current();                            \
    if (!CL_CHECK_THREAD(thread)) {                                          \
        return CL_OUT_OF_HOST_MEMORY;                                        \
    }

#define RUNTIME_ENTRY_VOID(ret, func, args)                                  \
CL_API_ENTRY ret CL_API_CALL                                                 \
func args                                                                    \
{                                                                            \
    amd::Thread* thread = amd::Thread::current();                            \
    if (!CL_CHECK_THREAD(thread)) {                                          \
        return;                                                              \
    }

#define RUNTIME_EXIT                                                         \
    /* FIXME_lmoriche: we should check to thread->lastError here! */         \
}

//! Helper function to check "properties" parameter in various functions
int checkContextProperties(
    const cl_context_properties *properties,
    bool*   offlineDevices);

namespace amd {

namespace detail {

template <typename T>
struct ParamInfo
{
    static inline std::pair<const void*, size_t> get(const T& param) {
        return std::pair<const void*, size_t>(&param, sizeof(T));
    }
};

template <>
struct ParamInfo<const char*>
{
    static inline std::pair<const void*, size_t> get(const char* param) {
        return std::pair<const void*, size_t>(param, strlen(param) + 1);
    }
};

template <int N>
struct ParamInfo<char[N]>
{
    static inline std::pair<const void*, size_t> get(const char* param) {
        return std::pair<const void*, size_t>(param, strlen(param) + 1);
    }
};

} // namespace detail

template <typename T>
static inline cl_int
clGetInfo(
    T& field,
    size_t param_value_size,
    void* param_value,
    size_t* param_value_size_ret)
{
    const void *valuePtr;
    size_t valueSize;

    std::tie(valuePtr, valueSize)
        = detail::ParamInfo<typename std::remove_const<T>::type>::get(field);

    if (param_value != NULL && param_value_size < valueSize) {
        return CL_INVALID_VALUE;
    }

    *not_null(param_value_size_ret) = valueSize;
    if (param_value != NULL) {
        ::memcpy(param_value, valuePtr, valueSize);
        if (param_value_size > valueSize) {
            ::memset(static_cast<address>(param_value) + valueSize,
                '\0', param_value_size - valueSize);
        }
    }

    return CL_SUCCESS;
}

static inline cl_int
clSetEventWaitList(
    Command::EventWaitList& eventWaitList,
    const Context& context,
    cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list)
{
    if ((num_events_in_wait_list == 0 && event_wait_list != NULL)
            || (num_events_in_wait_list != 0 && event_wait_list == NULL)) {
        return CL_INVALID_EVENT_WAIT_LIST;
    }

    while (num_events_in_wait_list-- > 0) {
        cl_event event = *event_wait_list++;
        Event* amdEvent = as_amd(event);
        if (!is_valid(event)) {
            return CL_INVALID_EVENT_WAIT_LIST;
        }
        if (&context != &amdEvent->context()) {
            return CL_INVALID_CONTEXT;
        }
        eventWaitList.push_back(amdEvent);
    }
    return CL_SUCCESS;
}

//! Common function declarations for CL-external graphics API interop
cl_int clEnqueueAcquireExtObjectsAMD(cl_command_queue command_queue,
    cl_uint num_objects, const cl_mem* mem_objects,
    cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
    cl_event* event, cl_command_type cmd_type);
cl_int clEnqueueReleaseExtObjectsAMD(cl_command_queue command_queue,
    cl_uint num_objects, const cl_mem* mem_objects,
    cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
    cl_event* event, cl_command_type cmd_type);

// This may need moving somewhere tidier...

struct PlatformIDS { const struct KHRicdVendorDispatchRec* dispatch_; };
class PlatformID {
public:
    static PlatformIDS Platform;
};
#define AMD_PLATFORM (reinterpret_cast<cl_platform_id>(&amd::PlatformID::Platform))

#if cl_amd_open_video
cl_int clEnqueueVideoDecodeAMD(
    VideoSession&               session,
    cl_video_decode_data_amd*   video_data,
    cl_uint                     num_events_in_wait_list,
    const cl_event*             event_wait_list,
    cl_event*                   event);

cl_int clEnqueueVideoEncodeAMD(
    VideoSession&               session,
    cl_video_encode_data_amd*   video_data,
    cl_uint                     num_events_in_wait_list,
    const cl_event*             event_wait_list,
    cl_event*                   event);

#endif // cl_amd_open_video

} // namespace amd

extern "C" {

#ifdef cl_ext_device_fission

extern CL_API_ENTRY cl_int CL_API_CALL
clCreateSubDevicesEXT(
    cl_device_id in_device,
    const cl_device_partition_property_ext * partition_properties,
    cl_uint num_entries,
    cl_device_id * out_devices,
    cl_uint * num_devices);

extern CL_API_ENTRY cl_int CL_API_CALL
clRetainDeviceEXT(cl_device_id device);

extern CL_API_ENTRY cl_int CL_API_CALL
clReleaseDeviceEXT(cl_device_id device);

#else // cl_ext_device_fission

#define clCreateSubDevicesEXT ((void (*)(void))0)
#define clRetainDeviceEXT     ((void (*)(void))0)
#define clReleaseDeviceEXT    ((void (*)(void))0)

#endif // cl_ext_device_fission

extern CL_API_ENTRY cl_key_amd CL_API_CALL
clCreateKeyAMD(
    cl_platform_id platform,
    void (CL_CALLBACK * destructor)( void * ),
    cl_int * errcode_ret);

extern CL_API_ENTRY cl_int CL_API_CALL
clObjectGetValueForKeyAMD(
    void * object,
    cl_key_amd key,
    void ** ret_val);

extern CL_API_ENTRY cl_int CL_API_CALL
clObjectSetValueForKeyAMD(
    void * object,
    cl_key_amd key,
    void * value);

#if defined(CL_VERSION_1_1)
extern CL_API_ENTRY cl_int CL_API_CALL
clSetCommandQueueProperty(
    cl_command_queue command_queue,
    cl_command_queue_properties properties,
    cl_bool enable,
    cl_command_queue_properties *old_properties) CL_API_SUFFIX__VERSION_1_0;
#endif // CL_VERSION_1_1

#if cl_amd_open_video
extern CL_API_ENTRY cl_video_session_amd CL_API_CALL
clCreateVideoSessionAMD(
    cl_context                  context,
    cl_device_id                device,
    cl_video_session_flags_amd  flags,
    cl_video_config_type_amd    config_buffer_type,
    cl_uint                     config_buffer_size,
    void*                       config_buffer,
    cl_int*                     errcode_ret);

extern CL_API_ENTRY cl_int CL_API_CALL
clRetainVideoSessionAMD(
    cl_video_session_amd    video_session);

extern CL_API_ENTRY cl_int CL_API_CALL
clReleaseVideoSessionAMD(
    cl_video_session_amd    video_session);

extern CL_API_ENTRY cl_int CL_API_CALL
clGetVideoSessionInfoAMD(
    cl_video_session_amd        video_session,
    cl_video_session_info_amd   param_name,
    size_t                      param_value_size,
    void*                       param_value,
    size_t*                     param_value_size_ret);


extern CL_API_ENTRY cl_video_session_amd CL_API_CALL
clCreateVideoEncSessionAMD(
    cl_context                  context,
    cl_device_id                device,
    cl_video_session_flags_amd  flags,
    cl_video_config_type_amd    config_buffer_type,
    cl_uint                     config_buffer_size,
    void*                       config_buffer,
    cl_int*                     errcode_ret);


extern CL_API_ENTRY cl_int CL_API_CALL
clDestroyVideoEncSessionAMD(
    cl_video_session_amd video_session);


extern CL_API_ENTRY cl_int CL_API_CALL
clGetVideoSessionEncInfoAMD(
    cl_video_session_amd        video_session,
    cl_video_session_enc_info_amd   param_name,
    size_t                      param_value_size,
    void*                       param_value,
    size_t*                     param_value_size_ret);

extern CL_API_ENTRY cl_int CL_API_CALL
clSendEncodeConfigInfoAMD(
    cl_video_session_amd        video_session,
    size_t                      numBuffers,
    void*                       pConfigBuffers);

extern CL_API_ENTRY cl_int CL_API_CALL
clEnqueueRunVideoProgramAMD(
    cl_video_session_amd        video_session,
    void*                       video_data_struct,
    cl_uint                     num_events_in_wait_list,
    const cl_event*             event_wait_list,
    cl_event*                   event);

extern CL_API_ENTRY cl_int CL_API_CALL
clEncodeGetDeviceCapAMD(
    cl_device_id                device_id,
    cl_uint                     encode_mode,
    cl_uint                     encode_cap_total_size,
    cl_uint*                    num_encode_cap,
    void*                       pEncodeCAP);

#if 1
extern CL_API_ENTRY cl_int CL_API_CALL
clEncodePictureAMD(
    cl_video_session_amd        video_session,
    cl_uint                     number_of_encode_task_input_buffers,
    void*                       encode_task_input_buffer_list,
    void*                       picture_parameter,
    cl_uint*                    pTaskID);
#endif
extern CL_API_ENTRY cl_int CL_API_CALL
clEncodeQueryTaskDescriptionAMD(
      cl_video_session_amd      session,
      cl_uint                   num_of_task_description_request,
      cl_uint*                  num_of_task_description_return,
      void *                    task_description_list);

extern CL_API_ENTRY cl_int CL_API_CALL
clEncodeReleaseOutputResourceAMD(
      cl_video_session_amd      session,
      cl_uint                   task_id);

#endif // cl_amd_open_video

extern CL_API_ENTRY cl_mem CL_API_CALL
clConvertImageAMD(
    cl_context              context,
    cl_mem                  image,
    const cl_image_format * image_format,
    cl_int *                errcode_ret);

extern CL_API_ENTRY cl_mem CL_API_CALL
clCreateBufferFromImageAMD(
    cl_context              context,
    cl_mem                  image,
    cl_int *                errcode_ret);

} // extern "C"

//! \endcond

#endif /*CL_COMMON_HPP_*/