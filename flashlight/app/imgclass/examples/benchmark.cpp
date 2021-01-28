/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <iomanip>
#include <iostream>

#include "flashlight/ext/image/fl/models/ViT.h"

double timeit(std::function<void()> fn) {
  // warmup
  for (int i = 0; i < 10; ++i) {
    fn();
  }
  af::sync();

  int num_iters = 100;
  af::sync();
  auto start = af::timer::start();
  for (int i = 0; i < num_iters; i++) {
    fn();
  }
  af::sync();
  return af::timer::stop(start) * 1000.0 / num_iters;
}

int main(int argc, char** argv) {
  af::info();
  if (argc < 3) {
    std::cout
        << "Invalid arguments. Usage : <binary> <batchsize> <precision> <optim_level>"
        << std::endl;
    return 1;
  }

  if (std::stoi(argv[2]) == 16) {
    // Only set the optim mode to O1 if it was left empty
    std::cout << "Mixed precision training enabled. Will perform loss scaling."
              << std::endl;
    auto optim_level = std::stoi(argv[3]);
    if (optim_level == 1) {
      fl::OptimMode::get().setOptimLevel(fl::OptimLevel::O1);
    } else if (optim_level == 2) {
      fl::OptimMode::get().setOptimLevel(fl::OptimLevel::O2);
    } else if (optim_level == 3) {
      fl::OptimMode::get().setOptimLevel(fl::OptimLevel::O3);
    } else {
      fl::OptimMode::get().setOptimLevel(fl::OptimLevel::DEFAULT);
    }
  }

  auto network = std::make_shared<fl::ext::image::ViT>(
      12, // FLAGS_model_layers,
      768, // FLAGS_model_hidden_emb_size,
      3072, // FLAGS_model_mlp_size,
      12, // FLAGS_model_heads,
      0.1, // FLAGS_train_dropout,
      0.1, // FLAGS_train_layerdrop,
      1000);

  std::cout << "[Network] arch - " << network->prettyString() << std::endl;
  std::cout << "[Network] params - " << fl::numTotalParams(network)
            << std::endl;

  auto batch_size = std::stoi(argv[1]);
  auto input = fl::Variable(af::randu(224, 224, 3, batch_size), false);
  if (std::stoi(argv[2]) == 16) {
    input = input.as(af::dtype::f16);
  }

  // forward
  // network->eval();
  // auto fwd_fn = [&]() {
  //   network->zeroGrad();
  //   input.zeroGrad();
  //   auto output = network->forward({input});
  // };
  // std::cout << "Network fwd took " << timeit(fwd_fn) << "ms" << std::endl;

  // fwd + bwd
  network->train();
  auto fwd_bwd_fn = [&]() {
    network->zeroGrad();
    input.zeroGrad();
    auto output = network->forward({input}).front();
    output.backward();
  };
  auto batch_time = timeit(fwd_bwd_fn);
  std::cout << "Network fwd+bwd per batch: " << batch_time << "ms" << std::endl;
  std::cout << "Throughput: " << batch_size * 1000. / batch_time << " images/s"
            << std::endl;

  return 0;
}