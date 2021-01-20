
#include <ntifs.h>

#define ABS_TIME(Time) (Time)
#define RAV_TIME(Time) (-(Time))

#define DIV_TIME(Time) (Time)/(10 * 1000)

#define NANOSECONDS(Time) (((signed __int64)(Time)) / 100L)
#define MICROSECONDS(Time) (((signed __int64)(Time)) * NANOSECONDS(1000L))
#define MILLISECONDS(Time) (((signed __int64)(Time)) * MICROSECONDS(1000L))
#define SECONDS(Time) (((signed __int64)(Time)) * MILLISECONDS(1000L))

NTSYSAPI
NTSTATUS
NTAPI
ObReferenceObjectByName(
    __in PUNICODE_STRING objectName,
    __in ULONG Attributes,
    __in PACCESS_STATE PassedAccessState OPTIONAL,
    __in ACCESS_MASK DesiredAccess OPTIONAL,
    __in POBJECT_TYPE objectType,
    __in KPROCESSOR_MODE AccessMode,
    __inout PVOID ParseContext OPTIONAL,
    __out PVOID* Object
);

extern POBJECT_TYPE* IoDriverObjectType;

UNICODE_STRING gKernelConnectString = { 0 };
PDRIVER_DISPATCH gOldCondrvCreateMajorFunction = NULL;

//////////////////////////////////////////////////////////////////////////


__inline
VOID
OSLibSleep(
    __in ULONG WaitTime
)
{
    LARGE_INTEGER DueTime = { 0 };

    DueTime.QuadPart = RAV_TIME(MILLISECONDS(WaitTime));
    KeDelayExecutionThread(KernelMode, FALSE, &DueTime);

}

NTSTATUS
FilterCreateMajorFunction(
    __in struct _DEVICE_OBJECT* DeviceObject,
    __inout struct _IRP* Irp
)
{
    if (RtlCompareUnicodeString(&gKernelConnectString, &Irp->Tail.Overlay.CurrentStackLocation->FileObject->FileName, TRUE))
    {
        return gOldCondrvCreateMajorFunction(DeviceObject, Irp);
    }

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_NOT_FOUND;
    IofCompleteRequest(Irp, 0);

    return STATUS_NOT_FOUND;
}

VOID
ThreadProc(
    __in PVOID StartContext
)
{
    UNICODE_STRING String;
    PDRIVER_OBJECT CondrvDriverObject = NULL;

    RtlInitUnicodeString(&String, L"\\Driver\\condrv");

    while (TRUE)
    {
        if (NT_SUCCESS(ObReferenceObjectByName(
            &String,
            OBJ_CASE_INSENSITIVE,
            NULL,
            0,
            *IoDriverObjectType,
            KernelMode,
            NULL,
            &CondrvDriverObject
        )))
        {
            break;
        }

        OSLibSleep(1000);
    }

    gOldCondrvCreateMajorFunction = (PDRIVER_DISPATCH)InterlockedExchangePointer(
        (PVOID)& CondrvDriverObject->MajorFunction[IRP_MJ_CREATE],
        FilterCreateMajorFunction
    );

    PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS
DriverEntry(
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
)
{
    NTSTATUS Status;
    HANDLE ThreadHandle = NULL;

    RtlInitUnicodeString(&gKernelConnectString, L"\\KernelConnect");
    Status = PsCreateSystemThread(
        &ThreadHandle,
        0,
        NULL,
        NULL,
        NULL,
        ThreadProc,
        NULL
    );
    if (NT_SUCCESS(Status))
    {
        ZwClose(ThreadHandle);
    }

    return Status;
}