// for addon
#include <node.h>
#include <v8.h>
#include <nan.h>
#include <string>
#include <iostream>
#include <sstream>
#include "functions.h"
#include "listen.h"
#include "init.h"
#include "listenable.h"
#include "client.h"
#include "newclient.h"
#include "bactext.h"
#include "address.h"
#include "conversion.h"
#include "BacnetValue.h"


NAN_METHOD(objectTypeToString) {
    std::ostringstream errorStringStream;
    std::string inputString = extractString(info[0].As<v8::String>());
    if (info.Length() >= 1 && info[0]->IsString()) {
        unsigned index;
        if (bactext_object_type_index(extractString(info[0].As<v8::String>()).c_str(), &index)) {
            const char * name = bactext_object_type_name(index);
            info.GetReturnValue().Set(Nan::New(name).ToLocalChecked());
        } else {
            errorStringStream << "Object type string not valid" << ", provided : " << inputString;
            Nan::ThrowError(errorStringStream.str().c_str());
        }
    } else if (info.Length() >= 1 && info[0]->IsUint32()) {
        const char * name = bactext_object_type_name(info[0]->ToUint32()->Value());
        info.GetReturnValue().Set(Nan::New(name).ToLocalChecked());
    } else {
        errorStringStream << "Object type must be either a string or unsigned int" << ", provided : " << inputString;
        Nan::ThrowError(errorStringStream.str().c_str());
    }
}
NAN_METHOD(objectTypeToNumber) {
    std::ostringstream errorStringStream;
    std::string inputString = extractString(info[0].As<v8::String>());
    if (info.Length() >= 1) {
        unsigned value;
        const char * error = objectTypeToC(info[0], &value);
        if (error) {
            errorStringStream << "Object type string not valid : " << error << ", provided : " << inputString;
            Nan::ThrowError(errorStringStream.str().c_str());
        } else {
            info.GetReturnValue().Set(Nan::New(value));
        }
    } else {
        errorStringStream << "Object type must be either a string or unsigned int" << ", provided : " << inputString;
        Nan::ThrowError(errorStringStream.str().c_str());
    }
}
NAN_METHOD(propertyKeyToString) {
    std::ostringstream errorStringStream;
    std::string inputString = extractString(info[0].As<v8::String>());
    if (info.Length() >= 1 && info[0]->IsString()) {
        unsigned index;
        if (bactext_property_index(inputString.c_str(), &index)) {
            const char * name = bactext_property_name(index);
            info.GetReturnValue().Set(Nan::New(name).ToLocalChecked());
        } else {
            errorStringStream << "Property key string not valid" << ", provided : " << inputString;
            Nan::ThrowError(errorStringStream.str().c_str());
         }
    } else if (info.Length() >= 1 && info[0]->IsUint32()) {
        uint32_t propertyKey = info[0]->ToUint32()->Value();
        if (propertyKey <= MAX_BACNET_PROPERTY_ID) {
            const char * name = bactext_property_name(propertyKey);
            info.GetReturnValue().Set(Nan::New(name).ToLocalChecked());
        } else {
            errorStringStream << "Property key too large, maximum is 4194303" << ", provided : " << inputString;
            Nan::ThrowRangeError(errorStringStream.str().c_str());
        }
    } else {
        errorStringStream << "Property key must be either a string or unsigned int" << ", provided : " << inputString;
        Nan::ThrowError(errorStringStream.str().c_str());
    }
}
NAN_METHOD(propertyKeyToNumber) {
    std::ostringstream errorStringStream;
    std::string inputString = extractString(info[0].As<v8::String>());
    if (info.Length() >= 1 && info[0]->IsString()) {
        unsigned index;
        if (bactext_property_index(inputString.c_str(), &index)) {
            info.GetReturnValue().Set(Nan::New(index));
        } else {
            errorStringStream << "Property key string not valid" << ", provided : " << inputString;
            Nan::ThrowError(errorStringStream.str().c_str());
        }
    } else if (info.Length() >= 1 && info[0]->IsUint32()) {
        uint32_t propertyKey = info[0]->ToUint32()->Value();
        if (propertyKey <= MAX_BACNET_PROPERTY_ID) {
            info.GetReturnValue().Set(Nan::New(propertyKey));
        } else {
            errorStringStream << "Property key too large, maximum is 4194303" << ", provided : " << inputString;
            Nan::ThrowRangeError(errorStringStream.str().c_str());
        }
    } else {
        errorStringStream << "Property key must be either a string or unsigned int" << ", provided : " << inputString;
        Nan::ThrowError(errorStringStream.str().c_str());
    }
}

// whois([destination, [min_id , [max_id]]])
NAN_METHOD(whois) {
    int32_t min_id = -1;
    int32_t max_id = -1;
    BACNET_ADDRESS dest = bacnetAddressToC(info[0]);

    if (info.Length() > 1) {
        min_id = max_id = info[1]->ToInt32()->Value();
    }
    if (info.Length() > 2) {
        max_id = info[2]->ToInt32()->Value();
    }

    if (min_id > BACNET_MAX_INSTANCE) {
        fprintf(stderr, "device-instance-min=%u - it must be less than %u\n",
            min_id, BACNET_MAX_INSTANCE + 1);
        return;
    }
    if (max_id > BACNET_MAX_INSTANCE) {
        fprintf(stderr, "device-instance-max=%u - it must be less than %u\n",
            max_id, BACNET_MAX_INSTANCE + 1);
        return;
    }
    /* send the request */
    Send_WhoIs_To_Network(&dest, min_id,
        max_id);

    info.GetReturnValue().Set(Nan::New(true));
}

bool addressOrBoundDeviceIdToC(Local<Value> value, unsigned * max_apdu, BACNET_ADDRESS * dest) {
    if (value->IsNumber()) {  // device id
        int32_t device_id = value->ToInt32()->Value();

        /* is the device bound? */
        bool isBound = address_get_by_device(device_id, max_apdu, dest);
        if (!isBound) {
            Nan::ThrowError("device is not bound\n");
            return false;
        }
    } else {   // device address
        *dest = bacnetAddressToC(value);
        *max_apdu = MAX_APDU; // without doing the whois we dont know the Max apdu for the device - so we will just hope it is the same as ours
    }
    return true;
}

// readProperty(deviceId, objectType, objectId, property [, arrayIndex])
NAN_METHOD(readProperty) {
    BACNET_ADDRESS dest = {};
    unsigned max_apdu = 0;

    bool addressed = addressOrBoundDeviceIdToC(info[0], &max_apdu, &dest);
    int32_t object_type = info[1]->ToInt32()->Value();
    int32_t object_instance = info[2]->ToInt32()->Value();
    int32_t object_property = info[3]->ToInt32()->Value();
    uint32_t array_index = BACNET_ARRAY_ALL;

    if (info[4]->IsUint32()) {
        array_index = info[4]->ToUint32()->Value();
    }

    if (addressed) {
        int invoke_id = Send_Read_Property_Request_Address(
            &dest,
            max_apdu,
            (BACNET_OBJECT_TYPE)object_type,
            object_instance,
            (BACNET_PROPERTY_ID)object_property,
            array_index);

        info.GetReturnValue().Set(Nan::New(invoke_id));
    } else {
        Nan::ThrowError("Unable to resolve address for read.");
    }
}

// isBound(deviceId) - checks whether a deviceId is already bound and can be used to address a device
NAN_METHOD(isBound) {
    BACNET_ADDRESS dest = {};
    unsigned max_apdu = 0;

    int32_t device_id = info[0]->ToInt32()->Value();
    bool bound = address_get_by_device(device_id, &max_apdu, &dest);
    info.GetReturnValue().Set(Nan::New(bound));
}

// writeProperty(deviceId, objectType, objectId, property, arrayIndex, value [, priority])
NAN_METHOD(writeProperty) {
    BACNET_ADDRESS dest = {};
    unsigned max_apdu = 0;

    bool addressed = addressOrBoundDeviceIdToC(info[0], &max_apdu, &dest);
    int32_t object_type = info[1]->ToInt32()->Value();
    int32_t object_instance = info[2]->ToInt32()->Value();
    int32_t object_property = info[3]->ToInt32()->Value();
    uint32_t array_index = BACNET_ARRAY_ALL;

    uint8_t priority = info[6]->IsUndefined() ? BACNET_NO_PRIORITY : info[6]->ToInt32()->Value();

    if (info[4]->IsUint32()) {
        array_index = info[4]->ToUint32()->Value();
    }

    if (addressed) {
        Local<Object> valueObject = Nan::To<Object>(info[5]).ToLocalChecked();
        BacnetValue * bacnetValue = BacnetValue::Unwrap<BacnetValue>(valueObject);
        BACNET_APPLICATION_DATA_VALUE object_value = {};
        if (bacnetValue->bacnetValue(&object_value)) {
            int invoke_id = Send_Write_Property_Request_Address(
                &dest,
                max_apdu,
                (BACNET_OBJECT_TYPE)object_type,
                object_instance,
                (BACNET_PROPERTY_ID)object_property,
                &object_value,
                priority,
                array_index);
            info.GetReturnValue().Set(Nan::New(invoke_id));
        } else {
            Nan::ThrowError("Some kind of error converting bacnet value for write property");
        }
    } else {
        Nan::ThrowError("Unable to resolve address for write.");
    }
}

NAN_METHOD(listen) {
  listenLoop();
}

NAN_METHOD(initClient) {
  v8::Local<v8::Object> localEventEmitter = info[0]->ToObject();
  eventEmitterSet(localEventEmitter);

  init_service_handlers();
}

NAN_METHOD(initDevice) {
  init_device_service_handlers();
}

NAN_METHOD(closeQueue) {
  eventEmitterClose();
}
