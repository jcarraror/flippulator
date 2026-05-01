#include <core/record.h>
#include "storage.h"

FS_Error storage_int_backup(Storage* api, const char* dstname) {
    UNUSED(api);
    UNUSED(dstname);
    return FSE_OK;
}

FS_Error storage_int_restore(Storage* api, const char* srcname, Storage_name_converter converter) {
    UNUSED(api);
    UNUSED(srcname);
    UNUSED(converter);
    return FSE_OK;
}
