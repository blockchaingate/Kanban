#ifndef SECP256K1_CPP_H_header
#define SECP256K1_CPP_H_header
#include "../opencl/cl/secp256k1.h"
#include <sstream>
std::string toStringSecp256k1_FieldElement(const secp256k1_fe& input);
std::string toStringSecp256k1_Scalar(const secp256k1_scalar& input);
std::string toStringSecp256k1_ECPoint(const secp256k1_ge& input);
std::string toStringSecp256k1_ECPointProjective(const secp256k1_gej& input);

std::string toStringSecp256k1_ECPointStorage(const secp256k1_ge_storage& input);
std::string toStringSecp256k1_MultiplicationContext(const secp256k1_ecmult_context& multiplicationContext, bool fullDetail);
std::string toStringSecp256k1_GeneratorContext(const secp256k1_ecmult_gen_context& generatorContext, bool fullDetail);

std::string toStringOutputObject(int argumentIndex, const unsigned char* memoryPool);

std::string toStringErrorLog(const unsigned char* memoryPool);

#ifndef __kernel
#define __kernel
#endif
#ifndef __global
#define __global
#endif

__kernel void secp256k1_opencl_compute_multiplication_context(
  __global unsigned char* outputMemoryPoolContainingMultiplicationContext
);

__kernel void secp256k1_opencl_compute_generator_context(
  __global unsigned char* outputMemoryPoolContainingGeneratorContext
);

__kernel void secp256k1_opencl_sign(
  __global unsigned char* outputSignature,
  __global unsigned char* outputSize,
  __global unsigned char* outputInputNonce,
  __global unsigned char* inputSecretKey,
  __global unsigned char* inputMessage,
  __global unsigned char* inputMemoryPoolGeneratorContext
);

__kernel void secp256k1_opencl_generate_public_key(
  __global unsigned char* outputPublicKey,
  __global unsigned char* outputPublicKeySize,
  __global unsigned char* inputSecretKey,
  __global unsigned char* inputMemoryPoolGeneratorContext
);

__kernel void secp256k1_opencl_verify_signature(
  __global unsigned char *output,
  __global unsigned char *outputMemoryPoolSignature,
  __global const unsigned char *inputSignature,
  __global unsigned int signatureSize,
  __global const unsigned char *publicKey,
  __global unsigned int publicKeySize,
  __global const unsigned char *message,
  __global const unsigned char *memoryPoolMultiplicationContext
);
#endif //SECP256K1_CPP_H_header

