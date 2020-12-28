/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
  ==============================================================================*/

#include <TensorFlowLite.h>

#include "main_functions.h"

#include "accelerometer_handler.h"
#include "gesture_predictor.h"
#include "magic_wand_model_data.h"
#include "output_handler.h"
#include "tensorflow/lite/experimental/micro/kernels/micro_ops.h"
#include "tensorflow/lite/experimental/micro/micro_error_reporter.h"
#include "tensorflow/lite/experimental/micro/micro_interpreter.h"
#include "tensorflow/lite/experimental/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"
#include <ArduinoBLE.h>

// Globals, used for compatibility with Arduino-style sketches.
namespace {
tflite::ErrorReporter* error_reporter = nullptr;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* model_input = nullptr;
int input_length;

// Create an area of memory to use for input, output, and intermediate arrays.
// The size of this will depend on the model you're using, and may need to be
// determined by experimentation.
constexpr int kTensorArenaSize = 60 * 1024;
uint8_t tensor_arena[kTensorArenaSize];

// Whether we should clear the buffer next time we fetch data
bool should_clear_buffer = false;
}  // namespace

// BLE Magic Wand Service
BLEService magicWandService("33e8c28d-f16c-46aa-8d77-85608274807f");
// BLE Magic Wand Characteristic
BLEUnsignedCharCharacteristic magicWandGestureChar("155b8a1f-e774-4556-b01b-6723bddb1d65",  // custom 128-bit characteristic UUID
    BLERead | BLENotify); // remote clients will be able to get notifications if this characteristic changes
// Philips Hue Turn off/on
bool isLumosActive = false;

// The name of this function is important for Arduino compatibility.
void setup() {
  // Set up logging. Google style is to avoid globals or statics because of
  // lifetime uncertainty, but since this has a trivial destructor it's okay.
  static tflite::MicroErrorReporter micro_error_reporter;  // NOLINT
  error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  model = tflite::GetModel(g_magic_wand_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    error_reporter->Report(
      "Model provided is schema version %d not equal "
      "to supported version %d.",
      model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  // Pull in only the operation implementations we need.
  // This relies on a complete list of all the ops needed by this graph.
  // An easier approach is to just use the AllOpsResolver, but this will
  // incur some penalty in code space for op implementations that are not
  // needed by this graph.
  static tflite::MicroMutableOpResolver micro_mutable_op_resolver;  // NOLINT
  micro_mutable_op_resolver.AddBuiltin(
    tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
    tflite::ops::micro::Register_DEPTHWISE_CONV_2D());
  micro_mutable_op_resolver.AddBuiltin(
    tflite::BuiltinOperator_MAX_POOL_2D,
    tflite::ops::micro::Register_MAX_POOL_2D());
  micro_mutable_op_resolver.AddBuiltin(tflite::BuiltinOperator_CONV_2D,
                                       tflite::ops::micro::Register_CONV_2D());
  micro_mutable_op_resolver.AddBuiltin(
    tflite::BuiltinOperator_FULLY_CONNECTED,
    tflite::ops::micro::Register_FULLY_CONNECTED());
  micro_mutable_op_resolver.AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                                       tflite::ops::micro::Register_SOFTMAX());

  // Build an interpreter to run the model with
  static tflite::MicroInterpreter static_interpreter(
    model, micro_mutable_op_resolver, tensor_arena, kTensorArenaSize,
    error_reporter);
  interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors
  interpreter->AllocateTensors();

  // Obtain pointer to the model's input tensor
  model_input = interpreter->input(0);
  if ((model_input->dims->size != 4) || (model_input->dims->data[0] != 1) ||
      (model_input->dims->data[1] != 128) ||
      (model_input->dims->data[2] != kChannelNumber) ||
      (model_input->type != kTfLiteFloat32)) {
    error_reporter->Report("Bad input tensor parameters in model");
    return;
  }

  input_length = model_input->bytes / sizeof(float);

  TfLiteStatus setup_status = SetupAccelerometer(error_reporter);
  if (setup_status != kTfLiteOk) {
    error_reporter->Report("Set up failed\n");
  }

  //  // Setup for BLE peripheral
  //  Serial.begin(9600);    // initialize serial communication
  //  // begin initialization
  //  if (!BLE.begin()) {
  //    Serial.println("starting BLE failed!");
  //    while (1);
  //  }
  //  /* Set a local name for the BLE device
  //     This name will appear in advertising packets
  //     and can be used by remote devices to identify this BLE device
  //     The name can be changed but maybe be truncated based on space left in advertisement packet
  //  */
  //  BLE.setLocalName("MagicWand");
  //  BLE.setAdvertisedService(magicWandService); // add the service UUID
  //  magicWandService.addCharacteristic(magicWandGestureChar); // add the magic wand gesture characteristic
  //  BLE.addService(magicWandService); // Add the magic wand service
  //  /* Start advertising BLE.  It will start continuously transmitting BLE
  //     advertising packets and will be visible to remote BLE central devices
  //     until it receives a new connection */
  //  // start advertising
  //  BLE.advertise();
  //  Serial.println("Bluetooth device active, waiting for connections...");

  // Setup for BLE Central
  Serial.begin(9600);
  // configure the button pin as input
//  pinMode(buttonPin, INPUT);
  // initialize the BLE hardware
  BLE.begin();
  Serial.println("BLE Central - Magic Wandcontrol");
  // start scanning for peripherals
  BLE.scanForUuid("932c32bd-0000-47a2-835a-a8d455b859dd");
}

void loop() {
  // Attempt to read new data from the accelerometer
  bool got_data = ReadAccelerometer(error_reporter, model_input->data.f,
                                    input_length, should_clear_buffer);
  // Don't try to clear the buffer again
  should_clear_buffer = false;
  // If there was no new data, wait until next time
  if (!got_data) return;
  // Run inference, and report any error
  TfLiteStatus invoke_status = interpreter->Invoke();
  if (invoke_status != kTfLiteOk) {
    error_reporter->Report("Invoke failed on index: %d\n", begin_index);
    return;
  }
  // Analyze the results to obtain a prediction
  int gesture_index = PredictGesture(interpreter->output(0)->data.f);
  // Clear the buffer next time we read data
  should_clear_buffer = gesture_index < 3;
  //  BLE Peripheral
  //   wait for a BLE central
  //  BLEDevice central = BLE.central();
  //  // if a central is connected to the peripheral:
  //  if (central && should_clear_buffer) {
  //    Serial.print("Connected to central: ");
  //    // print the central's BT address:
  //    Serial.println(central.address());
  //    Serial.print("Magic Wand Gesture is now: "); // print it
  //    Serial.println(gesture_index);
  //    magicWandGestureChar.writeValue(gesture_index);  // and update the magic wand gesture characteristic
  //  }
  //  if (!central.connected()) {
  //    Serial.print("Disconnected from central: ");
  //    Serial.println(central.address());
  //  }
  //  // Produce an output
  //  HandleOutput(error_reporter, gesture_index);

  // BLE Central
  // check if a peripheral has been discovered
  BLEDevice peripheral = BLE.available();
  if (peripheral) {
    // discovered a peripheral, print out address, local name, and advertised service
    Serial.print("Found ");
    Serial.print(peripheral.address());
    Serial.print(" '");
    Serial.print(peripheral.localName());
    Serial.print("' ");
    Serial.print(peripheral.advertisedServiceUuid());
    Serial.println();
    if (peripheral.localName() != "Hue Lamp") {
      return;
    }
    // stop scanning
    BLE.stopScan();
    if(should_clear_buffer) {
       controlHueLumos(peripheral);
    }
    // peripheral disconnected, start scanning again
    BLE.scanForUuid("932c32bd-0000-47a2-835a-a8d455b859dd");
  }
  // Produce an output
  HandleOutput(error_reporter, gesture_index);
}

void controlHueLumos(BLEDevice peripheral) {
  // connect to the peripheral
  Serial.println("Connecting ...");
  if (peripheral.connect()) {
    Serial.println("Connected");
  } else {
    Serial.println("Failed to connect!");
    return;
  }
  // discover peripheral attributes
  Serial.println("Discovering attributes ...");
  if (peripheral.discoverAttributes()) {
    Serial.println("Attributes discovered");
  } else {
    Serial.println("Attribute discovery failed!");
    peripheral.disconnect();
    return;
  }
  // retrieve the LED characteristic
  BLECharacteristic ledCharacteristic = peripheral.characteristic("932c32bd-0002-47a2-835a-a8d455b859dd");
  if (!ledCharacteristic) {
    Serial.println("Peripheral does not have On/Off characteristic!");
    peripheral.disconnect();
    return;
  } else if (!ledCharacteristic.canWrite()) {
    Serial.println("Peripheral does not have a writable On/Off characteristic!");
    peripheral.disconnect();
    return;
  }
  while (peripheral.connected()) {
    // while the peripheral is connected
    // read the button pin
    //    int buttonState = digitalRead(buttonPin);
    //    if (oldButtonState != buttonState) {
    // button changed
    //      oldButtonState = buttonState;
    if (isLumosActive) {
      Serial.println("lumos on");
      // button is pressed, write 0x01 to turn the LED on
      ledCharacteristic.writeValue((byte)0x01);
      isLumosActive = true;
    } else {
      Serial.println("lumos off");
      // button is released, write 0x00 to turn the LED off
      ledCharacteristic.writeValue((byte)0x00);
      isLumosActive = false;
    }
    //  }
  }
  Serial.println("Peripheral disconnected");
}
