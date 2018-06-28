#ifndef OPENOLT_TRANSLATION_H_
#define OPENOLT_TRANSLATION_H_

#include <string>
extern "C"
{
#include <bal_model_types.h>
}

int interface_key_to_port_no(bcmbal_interface_key key);
std::string alarm_status_to_string(bcmbal_alarm_status status);



#endif
