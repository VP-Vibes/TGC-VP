/*
 * Copyright (c) 2019 -2021 MINRES Technolgies GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tgc_vp/system.h"

#ifdef USE_ETISS
#include "tlm/scc/tlm_id.h"
#include "tlm/scc/tlm_mm.h"
#include "tlm/scc/tlm_extensions.h"

#include "elfio/elfio.hpp"
#endif

namespace tgc_vp {
using namespace sc_core;
using namespace vpvper::sifive;
using namespace sysc::tgfs;

system::system(sc_core::sc_module_name nm)
    : sc_core::sc_module(nm),
      NAMED(router, platfrom_mmap.size() + 2, 1),
      NAMEDC(qspi0_ptr, spi, spi_impl::beh),
      NAMEDC(qspi1_ptr, spi, spi_impl::beh),
      NAMEDC(qspi2_ptr, spi, spi_impl::beh),
      qspi0(*qspi0_ptr),
      qspi1(*qspi1_ptr),
      qspi2(*qspi2_ptr) {
  auto& qspi0 = *qspi0_ptr;
  auto& qspi1 = *qspi1_ptr;
  auto& qspi2 = *qspi2_ptr;

#ifndef USE_ETISS
  core_complex.initiator(router.target[0]);
#endif

  size_t i = 0;
  for (const auto& e : platfrom_mmap) {
    router.initiator.at(i)(e.target);
    router.set_target_range(i, e.start, e.size);
    i++;
  }
  router.initiator.at(i)(mem_qspi.target);
  router.set_target_range(i, 0x20000000, 512_MB);
  router.initiator.at(++i)(mem_ram.target);
  router.set_target_range(i, 0x80000000, 128_kB);

  // etiss-core
#ifdef USE_ETISS
  etiss::cfg().set<bool>("jit.gcc.cleanup", true);
  etiss::cfg().set<bool>("jit.verify", false);
  etiss::cfg().set<bool>("etiss.load_integrated_libraries", true);
  etiss::cfg().set<bool>("jit.debug", false);
  // TODO: DMI supported on TGC-VP??
  etiss::cfg().set<bool>("etiss.enable_dmi", false);

  etiss::cfg().set<int>("etiss.max_block_size", 100);
  etiss::cfg().set<int>("ETISS::CPU_quantum_ps", 100000);
  // TODO: make sure below clk period is correct??
  etiss::cfg().set<int>("arch.cpu_cycle_time_ps", 14286);
  etiss::cfg().set<int>("vp.entry_point", 0x20400000);
  // TODO: can we do better than 1 word blocks
  etiss::cfg().set<int>("etiss.max_block_size", 1);

  etiss::cfg().set<std::string>("arch.cpu", "RISCV");
  etiss::cfg().set<std::string>("jit.type", "TCCJIT");

  // TODO: later remove
  etiss::cfg().set<int>("etiss.loglevel", 5);

  etiss_core_.rst_i_(rst_s);
  etiss_core_.setup();
  // TODO: what about clk, irqs??
  etiss_core_.data_sock_i_->bind(router.target[0]);
#endif

  uart1.clk_i(tlclk_s);
  qspi0.clk_i(tlclk_s);
  qspi1.clk_i(tlclk_s);
  qspi2.clk_i(tlclk_s);
  pwm0.clk_i(tlclk_s);
  pwm1.clk_i(tlclk_s);
  pwm2.clk_i(tlclk_s);
  gpio0.clk_i(tlclk_s);
  plic.clk_i(tlclk_s);
  aon.clk_i(tlclk_s);
  aon.lfclkc_o(lfclk_s);
  prci.hfclk_o(tlclk_s);  // clock driver
  clint.tlclk_i(tlclk_s);
  clint.lfclk_i(lfclk_s);

#ifndef USE_ETISS
  core_complex.clk_i(tlclk_s);
#endif

  uart0.rst_i(rst_s);
  uart1.rst_i(rst_s);
  qspi0.rst_i(rst_s);
  qspi1.rst_i(rst_s);
  qspi2.rst_i(rst_s);
  pwm0.rst_i(rst_s);
  pwm1.rst_i(rst_s);
  pwm2.rst_i(rst_s);
  gpio0.rst_i(rst_s);
  plic.rst_i(rst_s);
  aon.rst_o(rst_s);
  prci.rst_i(rst_s);
  clint.rst_i(rst_s);

#ifndef USE_ETISS
  core_complex.rst_i(rst_s);
#endif

  aon.erst_n_i(erst_n);

  clint.mtime_int_o(mtime_int_s);
  clint.msip_int_o(msie_int_s);

  plic.global_interrupts_i(global_int_s);
  plic.core_interrupt_o(core_int_s);

#ifndef USE_ETISS
  core_complex.sw_irq_i(msie_int_s);
  core_complex.timer_irq_i(mtime_int_s);
  core_complex.global_irq_i(core_int_s);
  core_complex.local_irq_i(local_int_s);
#endif

  pins_i(gpio0.pins_i);
  gpio0.pins_o(pins_o);

  uart0.irq_o(global_int_s[3]);

  gpio0.iof0_i[5](qspi1.sck_o);
  gpio0.iof0_i[3](qspi1.mosi_o);
  qspi1.miso_i(gpio0.iof0_o[4]);
  gpio0.iof0_i[2](qspi1.scs_o[0]);
  gpio0.iof0_i[9](qspi1.scs_o[2]);
  gpio0.iof0_i[10](qspi1.scs_o[3]);

  qspi0.irq_o(global_int_s[5]);
  qspi1.irq_o(global_int_s[6]);
  qspi2.irq_o(global_int_s[7]);

  s_dummy_sck_i[0](uart1.tx_o);
  uart1.rx_i(s_dummy_sck_o[0]);
  uart1.irq_o(global_int_s[4]);

  gpio0.iof1_i[0](pwm0.cmpgpio_o[0]);
  gpio0.iof1_i[1](pwm0.cmpgpio_o[1]);
  gpio0.iof1_i[2](pwm0.cmpgpio_o[2]);
  gpio0.iof1_i[3](pwm0.cmpgpio_o[3]);

  gpio0.iof1_i[10](pwm2.cmpgpio_o[0]);
  gpio0.iof1_i[11](pwm2.cmpgpio_o[1]);
  gpio0.iof1_i[12](pwm2.cmpgpio_o[2]);
  gpio0.iof1_i[13](pwm2.cmpgpio_o[3]);

  gpio0.iof1_i[19](pwm1.cmpgpio_o[0]);
  gpio0.iof1_i[20](pwm1.cmpgpio_o[1]);
  gpio0.iof1_i[21](pwm1.cmpgpio_o[2]);
  gpio0.iof1_i[22](pwm1.cmpgpio_o[3]);

  pwm0.cmpip_o[0](global_int_s[40]);
  pwm0.cmpip_o[1](global_int_s[41]);
  pwm0.cmpip_o[2](global_int_s[42]);
  pwm0.cmpip_o[3](global_int_s[43]);

  pwm1.cmpip_o[0](global_int_s[44]);
  pwm1.cmpip_o[1](global_int_s[45]);
  pwm1.cmpip_o[2](global_int_s[46]);
  pwm1.cmpip_o[3](global_int_s[47]);

  pwm2.cmpip_o[0](global_int_s[48]);
  pwm2.cmpip_o[1](global_int_s[49]);
  pwm2.cmpip_o[2](global_int_s[50]);
  pwm2.cmpip_o[3](global_int_s[51]);

  for (auto& sock : s_dummy_sck_i) sock.error_if_no_callback = false;
}

auto system::loadElfFile(std::string elf) -> void {
#ifdef USE_ETISS  
  // the implementation is inspired from J.Geier(TUMEDA) version used here:
  // https://github.com/tum-ei-eda/vrtlmod/blob/abd42379c42c6f0852c1dc338bd8e9dbf472f98b/test/benchmark/cv32e40p/sc_test.cpp#L173-L201

  ELFIO::elfio elf_reader{};
  auto load_status = elf_reader.load(elf);

  if (load_status == false) {
    throw std::runtime_error{"ELF file not loaded properly"};
  }

  std::vector<tlm::tlm_generic_payload*> flashmems{};
  int trans_id{0};
  for (auto& seg : elf_reader.segments) {
    auto seg_data{seg->get_data()};

    auto trans{tlm::scc::tlm_mm<tlm::tlm_base_protocol_types, false>::get().allocate<tlm::scc::data_buffer>(seg->get_file_size())};
    tlm::scc::setId(*trans, trans_id++);
    trans->set_streaming_width(seg->get_file_size());
    trans->set_byte_enable_ptr(NULL);
    trans->set_byte_enable_length(0);

    auto data{trans->get_data_ptr()};

    trans->set_address(seg->get_physical_address());
    trans->set_command(tlm::TLM_WRITE_COMMAND);
    for (size_t i=0; i<seg->get_file_size(); ++i) {
      data[i] = static_cast<uint8_t>(seg_data[i] & 0xff);
    }
    flashmems.push_back(trans);
  }

  for (auto& trans : flashmems) {
    trans->acquire();
    router.transport_dbg(0, *trans);
    if (trans->get_response_status() != tlm::TLM_OK_RESPONSE) {
      throw std::runtime_error{"ELF file not loaded properly while routing"};
    }
    trans->release();
  }  
#endif
}

}  // namespace tgc_vp
