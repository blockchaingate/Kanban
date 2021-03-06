#include "gpu.h"
#include "logging.h"
#include "miscellaneous.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <assert.h>
#include <chrono>

#define MAX_SOURCE_SIZE (0x100000)

Logger logGPU("../logfiles/logGPU.txt", "[GPU] ");


std::string OpenCLFunctions::getDeviceInfo(cl_device_id deviceId, cl_device_info informationRequested) {
  size_t outputBufferSize = 0;
  const size_t infoSize = 10000;
  char buffer[infoSize];
  clGetDeviceInfo(deviceId, informationRequested, infoSize, buffer, &outputBufferSize);
  std::string result;
  if (outputBufferSize > 0)
    result = std::string(buffer, outputBufferSize - 1);
  return result;
}

std::string OpenCLFunctions::getDriverVersion(cl_device_id deviceId) {
  return getDeviceInfo(deviceId, CL_DRIVER_VERSION);
}

bool OpenCLFunctions::getIsLittleEndian(cl_device_id deviceId) {
  std::string returnValue = getDeviceInfo(deviceId, CL_DEVICE_ENDIAN_LITTLE);
  bool result = (returnValue[0] == CL_TRUE);
  return result;
}

long long OpenCLFunctions::getGlobalMemorySize(cl_device_id deviceId) {
  const size_t infoSize = sizeof(cl_ulong);
  char buffer[infoSize];
  for (unsigned i = 0; i < infoSize; i ++)
    buffer[i] = 0;
  size_t outputBufferSize = 0;
  clGetDeviceInfo(deviceId, CL_DEVICE_GLOBAL_MEM_SIZE, infoSize, buffer, &outputBufferSize);
  long long result = 0;
  for (unsigned i = 8; i != 0; i --) {
    result *= 256;
    result += (int) ((unsigned char) buffer[i - 1]);
  }
  return result;
}

std::string OpenCLFunctions::getDeviceName(cl_device_id deviceId) {
  return (std::string) getDeviceInfo(deviceId, CL_DEVICE_NAME);
}

SharedMemory::SharedMemory() {
  this->name = "";
  this->theMemory = 0;
  this->typE = this->typeVoidPointer;
  this->uintValue = 0;
  this->memoryExternallyOwned = 0;
}

void SharedMemory::ReleaseMe() {
  clReleaseMemObject(this->theMemory);
  this->theMemory = 0;
  this->memoryExternallyOwned = 0;
  this->name = "";
}

SharedMemory::~SharedMemory() {
  this->ReleaseMe();
}

GPUKernel::GPUKernel() {
  this->local_item_size[0] = 32;
  this->global_item_size[0] = 32;
  this->local_item_size[1] = 1;
  this->global_item_size[1] = 1;
  this->local_item_size[2] = 1;
  this->global_item_size[2] = 1;
  this->numInitializedExternallyOwnedBuffers = 0;
  this->program = NULL;
  this->kernel = NULL;
  this->flagIsBuilt = false;
}

GPUKernel::~GPUKernel() {
  //logGPU << "Kernel " << this->name << " destruction started. " << Logger::endL;
  for (unsigned i = 0; i < this->inputs.size(); i ++) {
    this->inputs[i]->ReleaseMe();
  }
  for (unsigned i = 0; i < this->outputs.size(); i ++) {
    this->outputs[i]->ReleaseMe();
  }
  cl_int ret = CL_SUCCESS;
  bool isGood = true;
  if (this->program != NULL) {
    ret = clReleaseProgram(this->program);
  }
  if (ret != CL_SUCCESS) {
    logGPU << "Error with code: " << ret << " while releasing kernel " << this->name << ". " << Logger::endL;
    isGood = false;
  }
  this->program = NULL;
  if (this->kernel != NULL) {
    ret = clReleaseKernel(this->kernel);
  }
  if (ret != CL_SUCCESS) {
    logGPU << "Error with code: " << ret << " while releasing kernel " << this->name << ". " << Logger::endL;
    isGood = false;
  }
  this->kernel = NULL;
  if (isGood) {
    logGPU << "Kernel " << this->name << " destroyed successfully. " << Logger::endL;
  } else {
    logGPU << "Encountered errors while destroying kernel " << this->name << ". " << Logger::endL;
  }
}

GPU::GPU() {
  this->flagVerbose = false;
  this->flagInitializedPlatform = false;
  this->flagInitializedKernelsFull = false;
  this->flagInitializedKernelsNoBuild = false;
  this->commandQueue = NULL;
  this->context = NULL;
  this->flagMultiplicationContextComputed = false;
  this->flagGeneratorContextComputed = false;
  this->flagMultiplicationContextComputationSTARTED = false;
  this->flagGeneratorContextComputationSTARTED = false;

  this->bufferMultiplicationContext = new unsigned char [GPU::memoryMultiplicationContext];
  this->bufferTestSuite1BasicOperations = new unsigned char [GPU::memoryMultiplicationContext];
  this->bufferGeneratorContext = new unsigned char [GPU::memoryGeneratorContext];
  this->bufferSignature = new unsigned char [GPU::memorySignature];
  this->theDesiredDeviceType = CL_DEVICE_TYPE_GPU;
}

bool GPU::initializeAllFull() {
  if (!this->initializePlatform()) {
    return false;
  }
  if (!this->initializeKernelsFull())
    return false;
  return true;
}

std::shared_ptr<GPUKernel> GPU::getKernel(const std::string& kernelName) {
  if (!this->initializeAllNoBuild()) {
    logGPU << "Fatal error: failed to initialize kernels in a function where failure is not allowed. " << Logger::endL;
    assert(false);
  }
  if (this->theKernels.find(kernelName) == this->theKernels.end()) {
    logGPU << "Fatal error: " << kernelName << " is not a known kernel name" << Logger::endL;
    assert(false);
  }
  return this->theKernels[kernelName];
}

std::string GPU::getId() {
  std::stringstream out;
  if (this->theDesiredDeviceType == CL_DEVICE_TYPE_GPU) {
    out << "Graphics PU";
  } else {
    out << "OpenCL CPU";
  }
  if (this->deviceInfo != "") {
    out << ", " << this->deviceInfo;
  }
  return out.str();
}

bool GPU::finish() {
  cl_int ret = clFinish(this->commandQueue);
  if (ret != CL_SUCCESS) {
    logGPU << "Fatal error: failed to finish GPU queue. Return code: " << ret << ". " << Logger::endL;
    return false;
  }
  return true;
}

bool GPU::initializeAllNoBuild() {
  logGPU << "DEBUG: initializing all no build ... " << Logger::endL;
  if (!this->initializePlatform()) {
    return false;
  }
  logGPU << "DEBUG: initializing kernels no build ... " << Logger::endL;
  if (!this->initializeKernelsNoBuild()) {
    return false;
  }
  logGPU << "DEBUG: got to here ... " << Logger::endL;
  return true;
}

bool GPU::initializePlatform() {
  if (this->flagInitializedPlatform)
    return true;
  int debugWarningDisableCacheDuringDevelopmentOnly;
  //setenv("CUDA_CACHE_DISABLE", "1", 1);
  this->context = 0;
  cl_int ret = 0;
  ret = clGetPlatformIDs(2, this->platformIds, &this->numberOfPlatforms);
  if (ret != CL_SUCCESS) {
    logGPU << "Failed to get platforms." << Logger::endL;
    return false;
  }
  if (this->flagVerbose) {
    logGPU << "Number of platforms: " << this->numberOfPlatforms << "\n";
  }
  std::string deviceDescription = this->theDesiredDeviceType == CL_DEVICE_TYPE_CPU ? "CPU" : "GPU";
  for (unsigned i = 0; i < this->numberOfPlatforms; i ++) {
    ret = clGetDeviceIDs(this->platformIds[i], this->theDesiredDeviceType, 2, this->allDevices, &this->numberOfDevices);
    if (ret == CL_SUCCESS)
      break;
  }
  if (ret != CL_SUCCESS) {
    logGPU << "Failed to get device of type: " << deviceDescription << Logger::endL;
    return false;
  }
  if (this->flagVerbose) {
    logGPU << "Number of devices of type: " << deviceDescription << ": " << this->numberOfDevices << "\n";
  }
  this->currentDeviceId = this->allDevices[0];
  this->deviceInfo = OpenCLFunctions::getDeviceName(this->currentDeviceId);
  if (this->flagVerbose) {
    logGPU << "Device name: " << OpenCLFunctions::getDeviceName(this->currentDeviceId) << "\n";
    logGPU << "Driver version: " << OpenCLFunctions::getDriverVersion(this->currentDeviceId) << "\n";
    logGPU << "Is little endian: " << OpenCLFunctions::getIsLittleEndian(this->currentDeviceId) << "\n";
    logGPU << "Memory: " << OpenCLFunctions::getGlobalMemorySize(this->currentDeviceId) << "\n";
  }
  // Create an OpenCL context
  logGPU << "About to create GPU context ..." << Logger::endL;
  this->context = clCreateContext(NULL, 1, &this->currentDeviceId, NULL, NULL, &ret);
  logGPU << "Context created." << Logger::endL;
  if (ret != CL_SUCCESS) {
    logGPU << "Failed to create context." << Logger::endL;
    return false;
  }
  logGPU << "About to create GPU queue ..." << Logger::endL;
  this->commandQueue = clCreateCommandQueue(
    this->context,
    this->currentDeviceId,
    CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
    &ret
  );
  logGPU << "GPU queue created." << Logger::endL;
  if (ret != CL_SUCCESS) {
    logGPU << "Failed to create command queue." << Logger::endL;
    return false;
  }
  if (this->commandQueue == NULL) {
    logGPU << "Command queue is NULL." << Logger::endL;
    return false;
  }
  this->flagInitializedPlatform = true;
  return true;
}

bool GPU::initializeKernelsNoBuild() {
  if (this->flagInitializedKernelsNoBuild)
    return true;
  if (!this->initializePlatform())
    return false;

  if (!this->createKernelNoBuild(
    this->kernelSHA256,
    {"result"},
    {SharedMemory::typeVoidPointer},
    {"offsets", "lengths", "message", "messageIndex"},
    {SharedMemory::typeVoidPointer, SharedMemory::typeVoidPointer, SharedMemory::typeVoidPointer, SharedMemory::typeMessageIndex},
    {},
    {}
  )) {
    return false;
  }
  if (!this->createKernelNoBuild(
    this->kernelSHA256TwiceFetchBest,
    {"result"},
    {SharedMemory::typeVoidPointer},
    {"message", "messageIndex"},
    {SharedMemory::typeVoidPointer, SharedMemory::typeMessageIndex},
    {},
    {}
  )) {
    return false;
  }
  if (!this->createKernelNoBuild(
    this->kernelInitializeMultiplicationContext,
    {"outputMultiplicationContext"},
    {
      SharedMemory::typeVoidPointer,
    },
    {},
    {},
    {},
    {}
  )) {
    return false;
  }
  if (!this->createKernelNoBuild(
    this->kernelInitializeGeneratorContext,
    {"outputGeneratorContext"},
    {
      SharedMemory::typeVoidPointer,
    },
    {},
    {},
    {},
    {}
  )) {
    return false;
  }
  //openCL function arguments:
  //__global unsigned char *output,
  //__global unsigned char *outputMemoryPoolSignature,
  //__global const unsigned char* inputSignature,
  //__global const unsigned char* signatureSizes,
  //__global const unsigned char* publicKey,
  //__global const unsigned char* publicKeySizes,
  //__global const unsigned char* message,
  //__global const unsigned char* memoryPoolMultiplicationContext,
  //unsigned int messageIndex

  if (!this->createKernelNoBuild(
    this->kernelVerifySignature,
    {
      "output",
      "outputMemoryPoolSignature",
    },
    {
      SharedMemory::typeVoidPointer,
      SharedMemory::typeVoidPointer
    },
    {
      "inputSignature",
      "signatureSize",
      "publicKey",
      "publicKeySize",
      "message",
      "memoryPoolMultiplicationContext",
      "messageIndex"
    },
    {
      SharedMemory::typeVoidPointer,
      SharedMemory::typeVoidPointer,
      SharedMemory::typeVoidPointer,
      SharedMemory::typeVoidPointer,
      SharedMemory::typeVoidPointer,
      SharedMemory::typeVoidPointerExternalOwnership,
      SharedMemory::typeMessageIndex
    },
    {
      "outputMultiplicationContext"
    },
    {
      this->kernelInitializeMultiplicationContext
    }
  )) {
    return false;
  }
  if (!this->createKernelNoBuild(
    this->kernelGeneratePublicKey,
    {
      "outputPublicKey",
      "outputPublicKeySize"
    },
    {
      SharedMemory::typeVoidPointer,
      SharedMemory::typeVoidPointer,
    },
    {
      "inputSecretKey",
      "inputMemoryPoolGeneratorContext",
      "inputMessageIndex"
    },
    {
      SharedMemory::typeVoidPointer,
      SharedMemory::typeVoidPointerExternalOwnership,
      SharedMemory::typeMessageIndex
    },
    {
      "outputGeneratorContext"
    },
    {
      this->kernelInitializeGeneratorContext
    }
  )) {
    return false;
  }
  if (!this->createKernelNoBuild(
    this->kernelSign,
    {
      "outputSignature",
      "outputSize",
      "outputInputNonce"
    },
    {
      SharedMemory::typeVoidPointer,
      SharedMemory::typeVoidPointer,
      SharedMemory::typeVoidPointer
    },
    {
      "inputSecretKey",
      "inputMessage",
      "inputMemoryPoolGeneratorContext",
      "inputMessageIndex"
    },
    {
      SharedMemory::typeVoidPointer,
      SharedMemory::typeVoidPointer,
      SharedMemory::typeVoidPointerExternalOwnership,
      SharedMemory::typeMessageIndex,
    },
    {
      "outputGeneratorContext"
    },
    {
      this->kernelInitializeGeneratorContext
    }
  )) {
    return false;
  }
  if (!this->createKernelNoBuild(
    this->kernelTestBuffer,
    {"buffer"},
    {SharedMemory::typeVoidPointer},
    {"offsets", "messageIndex"},
    {SharedMemory::typeVoidPointer, SharedMemory::typeMessageIndex},
    {},
    {}
  )) {
    return false;
  }
  if (!this->createKernelNoBuild(
    this->kernelTestSuite1BasicOperations,
    {"outputMemoryPool"},
    {
      SharedMemory::typeVoidPointer,
    },
    {},
    {},
    {},
    {}
  )) {
    return false;
  }
  this->flagInitializedKernelsNoBuild = true;
  return true;
}

bool GPU::initializeKernelsFull() {
  if (this->flagInitializedKernelsFull) {
    return true;
  }
  if (!this->initializePlatform()) {
    return false;
  }
  if (!this->initializeKernelsNoBuild()) {
    return false;
  }
  for (
    std::unordered_map<std::string, std::shared_ptr<GPUKernel> >::iterator kernelIterator = this->theKernels.begin();
    kernelIterator != this->theKernels.end();
    ++kernelIterator
  ) {
    if (!kernelIterator->second.get()->build()) {
      return false;
    }
  }
  return true;
}

bool GPU::createKernelNoBuild(
  const std::string& fileNameNoExtension,
  const std::vector<std::string>& outputs,
  const std::vector<int>& outputTypes,
  const std::vector<std::string>& inputs,
  const std::vector<int>& inputTypes,
  const std::vector<std::string>& inputExternalBufferNames,
  const std::vector<std::string>& inputExternalBufferKernelOwners
) {
  std::shared_ptr<GPUKernel> incomingKernel = std::make_shared<GPUKernel>();
  if (inputs.size() != inputTypes.size() || outputs.size() != outputTypes.size()) {
    logGPU << "Error: while initializing: " << fileNameNoExtension << ", got "
    << " non-matching number of kernel arguments and kernel argument types, namely "
    << inputs.size() << " inputs, " << inputTypes.size() << " input types, "
    << outputs.size() << " outputs, " << outputTypes.size() << " output types. " << Logger::endL;
    assert(false);
  }
  if (!incomingKernel->constructFromFileNameNoBuild(
    fileNameNoExtension,
    outputs,
    outputTypes,
    inputs,
    inputTypes,
    inputExternalBufferNames,
    inputExternalBufferKernelOwners,
    *this
  )) {
    return false;
  }
  this->theKernels[fileNameNoExtension] = incomingKernel;
  return true;
}

GPU::~GPU() {
  cl_int ret = CL_SUCCESS;
  if (this->commandQueue != NULL) {
    ret = clFlush(this->commandQueue);
    if (ret != CL_SUCCESS) {
      logGPU << "GPU destruction failure with error code: " << ret << ". " << Logger::endL;
    }
  }
  if (this->commandQueue != NULL) {
    ret = clFinish(this->commandQueue);
    if (ret != CL_SUCCESS) {
      logGPU << "GPU destruction failure with error code: " << ret << ". " << Logger::endL;
    }
  }
  if (this->commandQueue != NULL) {
    ret = clReleaseCommandQueue(this->commandQueue);
    if (ret != CL_SUCCESS) {
      logGPU << "GPU destruction failure with error code: " << ret << ". " << Logger::endL;
    }
  }
  this->commandQueue = NULL;
  if (this->context != NULL) {
    ret = clReleaseContext(this->context);
    if (ret!= CL_SUCCESS) {
      logGPU << "GPU destruction failure with error code: " << ret << ". " << Logger::endL;
    }
  }
  //logGPU << "GPU destruction: released context. " << Logger::endL;
  this->context = NULL;
  delete [] this->bufferMultiplicationContext;
  this->bufferMultiplicationContext = 0;
  delete [] this->bufferTestSuite1BasicOperations;
  this->bufferTestSuite1BasicOperations = 0;
  delete [] this->bufferGeneratorContext;
  this->bufferGeneratorContext = 0;
  delete [] this->bufferSignature;
  this->bufferSignature = 0;
  logGPU << "GPU destruction complete. " << Logger::endL;
}

std::string GPU::kernelSHA256 = "sha256GPU";
std::string GPU::kernelSHA256TwiceFetchBest = "sha256_twice_GPU_fetch_best";
std::string GPU::kernelTestBuffer = "testBuffer";
std::string GPU::kernelInitializeMultiplicationContext = "secp256k1_opencl_compute_multiplication_context";
std::string GPU::kernelInitializeGeneratorContext = "secp256k1_opencl_compute_generator_context";
std::string GPU::kernelVerifySignature = "secp256k1_opencl_verify_signature";
std::string GPU::kernelTestSuite1BasicOperations = "test_suite_1_basic_operations";
std::string GPU::kernelSign = "secp256k1_opencl_sign";
std::string GPU::kernelGeneratePublicKey = "secp256k1_opencl_generate_public_key";

const int maxProgramBuildBufferSize = 10000000;
char programBuildBuffer[maxProgramBuildBufferSize];

bool GPUKernel::hasArgumentName(const std::string& desiredArgumentName) {
  for (int k = 0; k < 2; k ++) {
    std::vector<std::string>& argumentNames = k == 0 ? this->desiredOutputNames : this->desiredInputNames;
    for (unsigned j = 0; j < argumentNames.size(); j ++) {
      if (argumentNames[j] == desiredArgumentName) {
        return true;
      }
    }
  }
  return false;
}

cl_mem* GPUKernel::getClMemPointer(const std::string& bufferName) {
  if (this->outputs.size() != this->desiredOutputNames.size()) {
    logGPU << "Kernel " << this->name << " does not have its output cl_mem buffers initialized "
    << " in function getClMemPointer." << Logger::endL;
    assert(false);
  }
  for (unsigned j = 0; j < this->outputs.size(); j ++) {
    if (this->desiredOutputNames[j] == bufferName) {
      return &this->outputs[j]->theMemory;
    }
  }
  if (this->inputs.size() != this->desiredInputNames.size()) {
    logGPU << "Kernel " << this->name << " does not have its input cl_mem buffers initialized "
    << " in function getClMemPointer." << Logger::endL;
    assert(false);
  }
  for (unsigned j = 0; j < this->inputs.size(); j ++) {
    if (this->desiredInputNames[j] == bufferName) {
      return &this->inputs[j]->theMemory;
    }
  }
  logGPU << "Kernel " << this->name << " is asked to deliver cl_mem buffer named: "
  << bufferName << " but no such buffer name has been declared. " << Logger::endL;
  assert(false);
  return 0;
}

bool GPUKernel::constructFromFileNameNoBuild(
  const std::string& fileNameNoExtension,
  const std::vector<std::string>& outputNames,
  const std::vector<int>& outputTypes,
  const std::vector<std::string>& inputNames,
  const std::vector<int>& inputTypes,
  const std::vector<std::string>& inputExternalBufferNames,
  const std::vector<std::string>& inputExternalBufferKernelOwners,
  GPU& ownerGPU
) {
  this->owner = &ownerGPU;
  this->name = fileNameNoExtension;
  std::string fileName = "../opencl/cl/" + fileNameNoExtension + ".cl";

  this->desiredOutputNames = outputNames;
  this->desiredOutputTypes = outputTypes;
  this->desiredInputNames = inputNames;
  this->desiredInputTypes = inputTypes;
  this->desiredExternalBufferNames = inputExternalBufferNames;
  this->desiredExternalBufferKernelOwners = inputExternalBufferKernelOwners;
  if (inputExternalBufferNames.size() != inputExternalBufferKernelOwners.size()) {
    logGPU << "External kernels and buffer names arrays must have the same size. " << Logger::endL;
    assert(false);
  }
  for (unsigned i = 0; i < this->desiredExternalBufferNames.size(); i ++) {
    const std::string& currentBuffer = this->desiredExternalBufferNames[i];
    const std::string& otherKernelName = this->desiredExternalBufferKernelOwners[i];
    if (this->owner->theKernels.find(otherKernelName) == this->owner->theKernels.end()) {
      logGPU << "Kernel " << this->name << " depends on kernel " << otherKernelName
      << " which has not been initialized yet/does not exist. " << Logger::endL;
      assert(false);
    }
    GPUKernel& otherKernel = *this->owner->theKernels[otherKernelName].get();
    if (!otherKernel.hasArgumentName(currentBuffer)) {
      logGPU << "Kernel " << this->name << " depends on buffer "
      << currentBuffer << " from kernel " << otherKernelName
      << " but that kernel appears to not contain a buffer with that name. " << Logger::endL;
      assert(false);
    }
  }

  std::ifstream theFile(fileName);
  if (!theFile.is_open()) {
    logGPU << "Failed to open " << fileName << "\n";
    return false;
  }
  std::string source_str((std::istreambuf_iterator<char>(theFile)), std::istreambuf_iterator<char>());
  if (this->owner->flagVerbose) {
    logGPU << "Program file name: " << fileName << "\n";
  }
  logGPU << "Source file read: " << fileName << Logger::endL;
  size_t sourceSize = source_str.size();
  const char* sourceCString = source_str.c_str();
  cl_int ret;
  this->program = clCreateProgramWithSource(
    this->owner->context, 1,
    (const char **)& sourceCString,
    (const size_t *)& sourceSize, &ret
  );
  if (ret != CL_SUCCESS) {
    logGPU << "Failed to create program from source. " << Logger::endL;
    return false;
  }
  //std::string programOptions = "-cl-opt-disable";

  //std::string programOptions = "-cl-std=CL2.0";
  return true;
}

std::vector<std::shared_ptr<SharedMemory> >& GPUKernel::getOutputCollection() {
  if (!this->build()) {
    logGPU << "Fatal error: requesting outputs of kernel " << this->name << " but it did not build successfully. " << Logger::endL;
    assert(false);
  }
  return this->outputs;
}

std::vector<std::shared_ptr<SharedMemory> >& GPUKernel::getInputCollection() {
  if (!this->build()) {
    logGPU << "Fatal error: requesting inputs of kernel " << this->name << " but it did not build successfully. " << Logger::endL;
    assert(false);
  }
  return this->inputs;
}

std::shared_ptr<SharedMemory>& GPUKernel::getOutput(int outputIndex) {
  std::vector<std::shared_ptr<SharedMemory> >& theOutputs = this->getOutputCollection();
  if (outputIndex < 0 || outputIndex >= (signed) theOutputs.size()) {
    logGPU << "Fatal error: requested output index " << outputIndex << " is out of bounds (outputs' size: "
    << theOutputs.size() << ")." << Logger::endL;
    assert(false);
  }
  return theOutputs[outputIndex];
}

std::shared_ptr<SharedMemory>& GPUKernel::getInput(int inputIndex) {
  std::vector<std::shared_ptr<SharedMemory> >& theInputs = this->getInputCollection();
  if (inputIndex < 0 || inputIndex >= (signed) theInputs.size()) {
    logGPU << "Fatal error: requested input index " << inputIndex << " is out of bounds (inputs' size: "
    << theInputs.size() << ")." << Logger::endL;
    assert(false);
  }
  return theInputs[inputIndex];
}

bool GPUKernel::build() {
  if (this->flagIsBuilt){
    return true;
  }
  StateMaintainerFolderLocation preserveCurrentFolder(logGPU);
  logGPU << this->owner->getId() << ": building program: " << this->name << "..." << Logger::endL;
  try {
    OSWrapper::setCurrentPath("../opencl/cl");
  } catch (...) {
    logGPU << Logger::colorYellow << "Failed to set folder: " << "../opencl/cl" << Logger::colorNormal << Logger::endL;
    logGPU << "This may be OK if current folder is already correct: " << Logger::colorBlue
    << OSWrapper::getCurrentPath() << Logger::colorNormal << Logger::endL;
  }
  auto timeStart = std::chrono::system_clock::now();
  std::string currentFolder = OSWrapper::getCurrentPath();
  logGPU << Logger::colorYellow << "Build base folder: " << currentFolder << Logger::colorNormal << Logger::endL;
  cl_int ret;
  std::stringstream programOptions;
  programOptions << "-I " << currentFolder;
  ret = clBuildProgram(
    this->program,
    1,
    &this->owner->currentDeviceId,
    //NULL,
    programOptions.str().c_str(),
    NULL,
    NULL
  );
  if (ret != CL_SUCCESS) {
    logGPU << "Failed to build program: " << this->name << ". Return code: " << ret << Logger::endL;
    size_t logSize;
    ret = clGetProgramBuildInfo(
      this->program, this->owner->currentDeviceId,
      CL_PROGRAM_BUILD_LOG, maxProgramBuildBufferSize,
      &programBuildBuffer, &logSize
    );
    if (ret != CL_SUCCESS) {
      logGPU << "Failed to fetch the build info for program: " << this->name << ". Return code: " << ret << Logger::endL;
      return false;
    }
    if (logSize > 0)
      logSize --;
    std::string theLog(programBuildBuffer, logSize);
    logGPU << theLog;
    return false;
  }
  auto timeAfterBuild = std::chrono::system_clock::now();
  std::chrono::duration<double> elapsed_seconds = timeAfterBuild - timeStart;
  logGPU << "Program built in " << elapsed_seconds.count() << " second(s)."  << Logger::endL;
  logGPU << "Creating openCL kernel..." << Logger::endL;
  this->kernel = clCreateKernel(this->program, this->name.c_str(), &ret);
  if (ret != CL_SUCCESS) {
    logGPU << "Failed to allocate kernel: " << this->name << ". Return code: " << ret << Logger::endL;
    logGPU << "Please note we \e[31mrequire the __kernel function name be the same\e[39m as the no-extension filename: \e[31m"
           << this->name << "\e[39m." << Logger::endL;
    return false;
  }
  logGPU << "Kernel: " << this->name << " created, allocating buffers..." << Logger::endL;
  this->buffersExternallyOwned.clear();
  for (unsigned i = 0; i < this->desiredExternalBufferNames.size(); i ++) {
    const std::string& otherKernelName = this->desiredExternalBufferKernelOwners[i];
    GPUKernel& other = *this->owner->theKernels[otherKernelName].get();
    if (!other.flagIsBuilt) {
      logGPU << "Initializing kernel " << this->name << " requires that " << otherKernelName
      << " be built. Proceeding to do that for you. " << Logger::endL;
      if (!other.build()) {
        return false;
      }
    }
    this->buffersExternallyOwned.push_back(other.getClMemPointer(this->desiredExternalBufferNames[i]));
  }
  this->constructArguments(this->desiredOutputNames, this->desiredOutputTypes, true, true);
  this->constructArguments(this->desiredInputNames, this->desiredInputTypes, true, false);
  if (!this->SetArguments()) {
    logGPU << "Failed to initialize arguments for kernel: " << this->name << Logger::endL;
    return false;
  }
  logGPU << "Kernel: " << this->name << " created successfully. " << Logger::endL;
  this->flagIsBuilt = true;
  return true;
}


bool GPUKernel::constructArguments(
  const std::vector<std::string>& argumentNames,
  const std::vector<int> &argumentTypes,
  bool isInput, bool isOutput
) {
  std::vector<std::shared_ptr<SharedMemory> >& theArgs = isOutput ? this->outputs : this->inputs;
  cl_int ret = CL_SUCCESS;
  cl_mem_flags bufferFlag = CL_MEM_READ_WRITE;
  if (isInput && isOutput) {
    bufferFlag = CL_MEM_READ_WRITE;
  }
  if (isInput && !isOutput) {
    bufferFlag = CL_MEM_READ_ONLY;
  }
  if (!isInput && isOutput ) {
    bufferFlag = CL_MEM_WRITE_ONLY;
  }
  if (!isInput && !isOutput) {
    logGPU << "GPU kernel arguments are neither input nor output" << Logger::endL;
    return false;
  }
  //bufferFlag |= CL_MEM_ALLOC_HOST_PTR;
  if (theArgs.size() != 0) {
    logGPU << "Fatal error: arguments not empty. " << Logger::endL;
    return false;
  }
  for (unsigned i = 0; i < argumentNames.size(); i ++) {
    theArgs.push_back(std::make_shared<SharedMemory>());
    std::shared_ptr<SharedMemory> current = theArgs[theArgs.size() - 1];
    current->name = argumentNames[i];
    current->typE = argumentTypes[i];
    if (current->typE != current->typeVoidPointer) {
      continue;
    }
    int PleaseRefactor;
    int bufferSize = GPU::defaultBufferSize;
    if (current->name == "outputMemoryPoolSignature") {
      bufferSize = MACRO_size_signature_buffer;
    }
    current->theMemory = clCreateBuffer(this->owner->context, bufferFlag, bufferSize, NULL, &ret);
    current->buffer.resize(GPU::defaultBufferSize);
    if (ret != CL_SUCCESS || current->theMemory == NULL) {
      logGPU << "Failed to create buffer \e[31m" << current->name << "\e[39m. Return code: " << ret << Logger::endL;
      return false;
    }
  }
  return true;
}

bool GPUKernel::SetArguments() {
  if (!this->SetArguments(this->outputs, 0))
    return false;
  if (!this->SetArguments(this->inputs, this->outputs.size()))
    return false;
  return true;
}

bool GPUKernel::SetArguments(std::vector<std::shared_ptr<SharedMemory> >& theArgs, unsigned offset) {
  //std::cout << "DEBUG: kernel: setting " << theArgs.size() << " arguments. "<< std::endl;
  cl_int ret = CL_SUCCESS;
  for (unsigned i = 0; i < theArgs.size(); i ++) {
    std::shared_ptr<SharedMemory> current = theArgs[i];
    if (current->typE == SharedMemory::typeVoidPointerExternalOwnership) {
      if (this->numInitializedExternallyOwnedBuffers >= this->buffersExternallyOwned.size()) {
        logGPU << "Argument " << current->name << " is set to have externally owned buffer but the index of next external buffer,"
        << this->numInitializedExternallyOwnedBuffers << ", is out of bounds: total: "
        << this->buffersExternallyOwned.size() << " external buffers. ";
        return false;
      }
      current->memoryExternallyOwned = this->buffersExternallyOwned[this->numInitializedExternallyOwnedBuffers];
      this->numInitializedExternallyOwnedBuffers ++;
      ret = clSetKernelArg(this->kernel, i + offset, sizeof(cl_mem), (void *) current->memoryExternallyOwned);
    }
    if (current->typE == SharedMemory::typeVoidPointer)
    { ret = clSetKernelArg(
        this->kernel, i + offset, sizeof(cl_mem), (void *)& current->theMemory
      );
    }
    if (current->typE == SharedMemory::typeMessageIndex) {
      if (! this->writeMessageIndex(current->uintValue)) {
        return false;
      }
    }
    if (ret != CL_SUCCESS) {
      logGPU << Logger::colorRed << "Failed to set kernel argument " << current->name << " with return code: "
      << ret << "." << Logger::colorNormal << Logger::endL;
    }
  }
  return true;
}

bool GPUKernel::writeToBuffer(unsigned argumentNumber, const std::vector<unsigned int>& input) {
  logGPU << "WRITING vector uint " << Logger::endL;

  std::vector<unsigned char> converted;
  converted.resize(input.size() * 4);
  for (unsigned i = 0; i < input.size(); i ++) {
    memoryPool_write_uint(input[i], &converted[i * 4]);
  }
  return this->writeToBuffer(argumentNumber, converted);
}

bool GPUKernel::writeToBuffer(unsigned argumentNumber, const std::vector<unsigned char>& input) {
  std::cout << std::hex << "About to write to buffer: address: " << (long) (& (input[0])) << std::endl;
  return this->writeToBuffer(argumentNumber, &(input[0]), input.size());
}

bool GPUKernel::writeToBuffer(unsigned argumentNumber, const std::vector<char>& input) {
  std::cout << std::hex << "About to write to buffer: address: " << (long) (& (input[0])) << std::endl;
  return this->writeToBuffer(argumentNumber, &(input[0]), input.size());
}

bool GPUKernel::writeToBuffer(unsigned argumentNumber, const std::string& input) {
  logGPU << "WRITING STRING " << Logger::endL;
  return this->writeToBuffer(argumentNumber, input.c_str(), input.size());
}

bool GPUKernel::writeToBuffer(unsigned argumentNumber, const void* inputBuffer, size_t size) {
  std::cout << std::dec << "DEBUG: writing VOID POINTER " << inputBuffer << ", size: " << size << std::endl;
  //std::cout << " in buffeR: " << &bufferToWriteInto << std::endl;
  cl_mem& bufferToWriteInto =
    argumentNumber < this->outputs.size() ?
    this->outputs[argumentNumber]->theMemory :
    this->inputs[argumentNumber - this->outputs.size()]->theMemory;
  cl_int ret = clEnqueueWriteBuffer(
    this->owner->commandQueue,
    bufferToWriteInto,
    CL_TRUE,
    0,
    size,
    inputBuffer,
    0,
    NULL,
    NULL
  );
  if (ret != CL_SUCCESS) {
    logGPU << "Enqueueing write buffer failed with input: " << inputBuffer << Logger::endL;
    return false;
  }
  return true;
}

std::vector<unsigned char> GPU::getUintBytesBigEndian(uint32_t input){
  std::vector<unsigned char> result;
  result.resize(4);
  result[0] = (input / 16777216) % 256;
  result[1] = (input / 65536   ) % 256;
  result[2] = (input / 256     ) % 256;
  result[3] = (input           ) % 256;
  return result;
}

bool GPUKernel::writeMessageIndex(uint inputArgument) {
  //std::cout << "DEBUG: writing " << input;
  //std::cout << "Setting: argument number: " << argumentNumber << ", input: " << input << std::endl;
  //The message index is always the last argument!
  int argumentNumber = this->outputs.size() + this->inputs.size() - 1;
  std::shared_ptr<SharedMemory>& currentArgument = this->inputs[this->inputs.size() - 1];
  //little/big endian agnostic:
  //Please note this code arose after multiple issues between openCL devices.
  //If you decide to fix to a more elegant solution,
  //please make sure to **test** on amd, intel CPUs
  //nvidia, intel, amd GPUS with openCL 1.1, 1.2 and 2.0.
  std::vector<unsigned char> theBytes = GPU::getUintBytesBigEndian(inputArgument);
  currentArgument->uintValue = inputArgument;
  cl_int ret3 = clSetKernelArg(this->kernel, argumentNumber    , 1, &theBytes[0]);
  cl_int ret2 = clSetKernelArg(this->kernel, argumentNumber + 1, 1, &theBytes[1]);
  cl_int ret1 = clSetKernelArg(this->kernel, argumentNumber + 2, 1, &theBytes[2]);
  cl_int ret0 = clSetKernelArg(this->kernel, argumentNumber + 3, 1, &theBytes[3]);
  if (
    ret3 != CL_SUCCESS ||
    ret2 != CL_SUCCESS ||
    ret1 != CL_SUCCESS ||
    ret0 != CL_SUCCESS
  ) {
    logGPU << "Set kernel arg failed. " << Logger::endL;
    return false;
  }
  return true;
}
