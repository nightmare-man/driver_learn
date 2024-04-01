#include <fltkernel.h>
#include <ntddk.h>


PFLT_FILTER gFilterHandle = NULL;
extern NTSTATUS ZwQueryInformationProcess(
	_In_ HANDLE ProcessHandle,
	_In_ PROCESSINFOCLASS ProcessInformationClass,
	_Out_ PVOID ProcessInformation,
	_In_ ULONG ProcessInformationLength,
	_Out_opt_ PULONG ReturnLength
);


NTSTATUS
DelProtectInstanceSetup(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
	_In_ DEVICE_TYPE VolumeDeviceType,
	_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
)
/*++

Routine Description:

	This routine is called whenever a new instance is created on a volume. This
	gives us a chance to decide if we need to attach to this volume or not.

	If this routine is not defined in the registration structure, automatic
	instances are always created.

Arguments:

	FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
		opaque handles to this filter, instance and its associated volume.

	Flags - Flags describing the reason for this attach request.

Return Value:

	STATUS_SUCCESS - attach
	STATUS_FLT_DO_NOT_ATTACH - do not attach

--*/
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(VolumeDeviceType);
	UNREFERENCED_PARAMETER(VolumeFilesystemType);



	return STATUS_SUCCESS;
}


NTSTATUS
DelProtectInstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
)
/*++

Routine Description:

	This is called when an instance is being manually deleted by a
	call to FltDetachVolume or FilterDetach thereby giving us a
	chance to fail that detach request.

	If this routine is not defined in the registration structure, explicit
	detach requests via FltDetachVolume or FilterDetach will always be
	failed.

Arguments:

	FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
		opaque handles to this filter, instance and its associated volume.

	Flags - Indicating where this detach request came from.

Return Value:

	Returns the status of this operation.

--*/
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);


	return STATUS_SUCCESS;
}


VOID
DelProtectInstanceTeardownStart(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
)
/*++

Routine Description:

	This routine is called at the start of instance teardown.

Arguments:

	FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
		opaque handles to this filter, instance and its associated volume.

	Flags - Reason why this instance is being deleted.

Return Value:

	None.

--*/
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();
}


VOID
DelProtectInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
)
/*++

Routine Description:

	This routine is called at the end of instance teardown.

Arguments:

	FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
		opaque handles to this filter, instance and its associated volume.

	Flags - Reason why this instance is being deleted.

Return Value:

	None.

--*/
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);


}

NTSTATUS
DelProtectUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
)
/*++

Routine Description:

	This is the unload routine for this miniFilter driver. This is called
	when the minifilter is about to be unloaded. We can fail this unload
	request if this is not a mandatory unload indicated by the Flags
	parameter.

Arguments:

	Flags - Indicating if this is a mandatory unload.

Return Value:

	Returns STATUS_SUCCESS.

--*/
{
	UNREFERENCED_PARAMETER(Flags);


	

	FltUnregisterFilter(gFilterHandle);

	return STATUS_SUCCESS;
}


FLT_PREOP_CALLBACK_STATUS pre_operation_before_del_by_create(_Inout_ PFLT_CALLBACK_DATA data, _In_ PCFLT_RELATED_OBJECTS file_objects, _Outptr_ PVOID* complete_context) {
	UNREFERENCED_PARAMETER(file_objects);
	UNREFERENCED_PARAMETER(complete_context);
	FLT_POSTOP_CALLBACK_STATUS ret_pre_call = FLT_PREOP_SUCCESS_NO_CALLBACK;
	if (data->RequestorMode == KernelMode) {
		return ret_pre_call;
	}
	if (data->Iopb->Parameters.Create.Options & FILE_DELETE_ON_CLOSE) {
		ULONG size = 300;
		PUNICODE_STRING process_name = (PUNICODE_STRING)ExAllocatePool(PagedPool, size);
		RtlZeroMemory(process_name, size);
		NTSTATUS ret = ZwQueryInformationProcess(NtCurrentProcess(), ProcessImageFileName, process_name, size - sizeof(WCHAR), NULL);
		//这个函数会自动将unicode字符写到process_name后面的空间，并将process_name的buffer指向正确的位置
		if (NT_SUCCESS(ret)) {
			if (wcsstr(process_name->Buffer, L"cmd.exe") != NULL || wcsstr(process_name->Buffer, L"CMD.EXE") != NULL) {
				data->IoStatus.Status = STATUS_ACCESS_DENIED;
				ret_pre_call = FLT_PREOP_COMPLETE;
			}
		}
		ExFreePool(process_name);
	}
	return ret_pre_call;
}

FLT_PREOP_CALLBACK_STATUS pre_operation_before_del_by_set(_Inout_ PFLT_CALLBACK_DATA data, _In_ PCFLT_RELATED_OBJECTS file_objects, _Outptr_ PVOID* complete_context) {
	UNREFERENCED_PARAMETER(file_objects);
	UNREFERENCED_PARAMETER(complete_context);
	FLT_POSTOP_CALLBACK_STATUS ret_pre_call = FLT_PREOP_SUCCESS_NO_CALLBACK;
	if (data->RequestorMode == KernelMode) {
		return ret_pre_call;
	}
	if (data->Iopb->Parameters.SetFileInformation.FileInformationClass!=FileDispositionInformation&&data->Iopb->Parameters.SetFileInformation.FileInformationClass!=FileDispositionInformationEx) {
		return ret_pre_call;
	}
	else {
		PFILE_DISPOSITION_INFORMATION info = (PFILE_DISPOSITION_INFORMATION)data->Iopb->Parameters.SetFileInformation.InfoBuffer;
		if (!info->DeleteFile) {
			return ret_pre_call;
		}
		else {
			//和IRP_MJ_CREATE不同的是，这里的设置信息的回调不一定是调用者线程执行的。
			PEPROCESS process= PsGetThreadProcess(data->Thread);
			NT_ASSERT(process);
			HANDLE h_process;
			NTSTATUS open_ret= ObOpenObjectByPointer(process, OBJ_KERNEL_HANDLE, NULL, 0, NULL, KernelMode, &h_process);
			if (!NT_SUCCESS(open_ret)) {
				return ret_pre_call;
			}

			ULONG size = 300;
			PUNICODE_STRING process_name = (PUNICODE_STRING)ExAllocatePool(PagedPool, size);
			RtlZeroMemory(process_name, size);
			NTSTATUS ret = ZwQueryInformationProcess(h_process, ProcessImageFileName, process_name, size - sizeof(WCHAR), NULL);
			//这个函数会自动将unicode字符写到process_name后面的空间，并将process_name的buffer指向正确的位置
			if (NT_SUCCESS(ret)) {
				if (wcsstr(process_name->Buffer, L"cmd.exe") != NULL || wcsstr(process_name->Buffer, L"CMD.EXE") != NULL) {
					data->IoStatus.Status = STATUS_ACCESS_DENIED;
					ret_pre_call = FLT_PREOP_COMPLETE;
				}
			}
			ExFreePool(process_name);

		}
	}
	return ret_pre_call;
}

NTSTATUS dirver_unload(_In_ PDRIVER_OBJECT driver_object) {
	UNREFERENCED_PARAMETER(driver_object);
	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT driver_object, _In_ PUNICODE_STRING register_path) {
	UNREFERENCED_PARAMETER(register_path);
	driver_object->DriverUnload = dirver_unload;
	FLT_OPERATION_REGISTRATION operations[] = { 
		{.MajorFunction=(UCHAR)IRP_MJ_CREATE,
		 .Flags=0,
		 .PreOperation=pre_operation_before_del_by_create,
		 .PostOperation=NULL
		},
		{
		 .MajorFunction= (UCHAR)IRP_MJ_SET_INFORMATION,
		 .Flags=0,
		 .PreOperation=pre_operation_before_del_by_set,
		 .PostOperation=NULL
		},
		{
			.MajorFunction=(UCHAR)IRP_MJ_OPERATION_END
		}
	};
	const FLT_REGISTRATION flt_reg = {
		.Size = sizeof(FLT_REGISTRATION),
		.Version=FLT_REGISTRATION_VERSION,
		.Flags=0,
		.ContextRegistration=NULL,
		.OperationRegistration=operations,
		.FilterUnloadCallback=NULL,
		.InstanceSetupCallback=DelProtectInstanceSetup,
		.InstanceQueryTeardownCallback=DelProtectInstanceQueryTeardown,
		.InstanceTeardownCompleteCallback= DelProtectInstanceTeardownComplete,
		.InstanceTeardownStartCallback=DelProtectInstanceTeardownStart
		//需要注意的是，其余几个回调函数也不能为空，否则不起作用
	};
	
	NTSTATUS ret = FltRegisterFilter(
		driver_object,
		&flt_reg,
		&gFilterHandle
	);
	if (NT_SUCCESS(ret)) {
		NTSTATUS ret1 = FltStartFiltering(gFilterHandle);
		if (!NT_SUCCESS(ret1)) {
			FltUnregisterFilter(gFilterHandle);
		}
	}
	return ret;
}