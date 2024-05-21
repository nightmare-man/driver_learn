#pragma once
#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)
#define VMM_DEVICE 0x8000
#define VMM_ON_OP CTL_CODE(VMM_DEVICE, 0x800, METHOD_NEITHER, FILE_ANY_ACCESS)
#define VMM_OFF_OP CTL_CODE(VMM_DEVICE, 0x801,METHOD_NEITHER, FILE_ANY_ACCESS)

