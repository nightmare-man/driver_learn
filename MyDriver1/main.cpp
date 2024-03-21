#include "ntddk.h"

void SampleUnload(_In_ PDRIVER_OBJECT driver_object) {
	UNREFERENCED_PARAMETER(driver_object);
	KdPrint(("Driver unload"));
}
extern "C"
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT driver_object, _In_ PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(driver_object);
	UNREFERENCED_PARAMETER(RegistryPath);
	driver_object->DriverUnload = SampleUnload;
	KdPrint(("Driver init"));
	RTL_OSVERSIONINFOW para;
	RtlGetVersion(&para);
	if (para.dwMajorVersion == 10 && para.dwMinorVersion == 0) {
		KdPrint(("is windows 10"));
	}
	return STATUS_SUCCESS;
}