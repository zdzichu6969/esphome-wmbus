#include "wmbusgw_component.h"

namespace esphome {
namespace wmbusgw {

static const char *TAG = "wM-Bus GW";

WMBusGwComponent::WMBusGwComponent() {}

WMBusGwComponent::~WMBusGwComponent() {}

void WMBusGwComponent::setup() {
  this->high_freq_.start();
  memset(this->mb_packet_, 0, sizeof(this->mb_packet_));
  rf_mbus_init(this->spi_conf_.mosi->get_pin(), this->spi_conf_.miso->get_pin(),
               this->spi_conf_.clk->get_pin(), this->spi_conf_.cs->get_pin(),
               this->spi_conf_.gdo0->get_pin(), this->spi_conf_.gdo2->get_pin());
}

void WMBusGwComponent::loop() {
  if ((millis() - this->last_connected_) > this->reboot_timeout_) {
    ESP_LOGE(TAG, "Can't send to clients... Restarting...");
    App.reboot();
  }
  int rssi_ = 0;
  if (rf_mbus_task(this->mb_packet_, rssi_, this->spi_conf_.gdo0->get_pin(), this->spi_conf_.gdo2->get_pin())) {
    uint8_t len_without_crc = crcRemove(this->mb_packet_, packetSize(this->mb_packet_[0]));
    ESP_LOGI(TAG, "T: %s", format_hex_pretty(this->mb_packet_, len_without_crc).c_str());

    for (auto & client : this->clients_) {
      if (client.format == Format::FORMAT_HEX) {
        if (this->tcp_client_.connect(client.ip.str().c_str(), client.port)) {
          this->tcp_client_.write((const uint8_t *) this->mb_packet_, len_without_crc);
          this->tcp_client_.stop();
          this->last_connected_ = millis();
        }
      }
      else if ((client.format == Format::FORMAT_RTLWMBUS)) {
        if (this->tcp_client_.connect(client.ip.str().c_str(), client.port)) {
          time_t current_time = this->time_->now().timestamp;
          strftime(telegram_time_, sizeof(telegram_time_), "%Y-%m-%d %H:%M:%S.000", localtime(&current_time));
          this->tcp_client_.printf("T1;1;1;%s;%d;;;0x", telegram_time_, rssi_);
          for (int i = 0; i < len_without_crc; i++){
            this->tcp_client_.printf("%02X", this->mb_packet_[i]);
          }
          this->tcp_client_.print("\n");
          this->tcp_client_.stop();
          this->last_connected_ = millis();
        }
      }
    }
    memset(this->mb_packet_, 0, sizeof(this->mb_packet_));
  }
}

const LogString *WMBusGwComponent::format_to_string(Format format) {
  switch (format) {
    case FORMAT_HEX:
      return LOG_STR("hex");
    case FORMAT_RTLWMBUS:
      return LOG_STR("rtl-wmbus");
    default:
      return LOG_STR("unknown");
  }
}

void WMBusGwComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Clients [%d]:", this->clients_.size());
  for (auto & client : this->clients_) {
    ESP_LOGCONFIG(TAG, "  %s: %s:%d [%s]",
                  client.name.c_str(),
                  client.ip.str().c_str(),
                  client.port,
                  LOG_STR_ARG(format_to_string(client.format)));
  }
  ESP_LOGCONFIG(TAG, "CC1101 SPI bus:");
  LOG_PIN("  MOSI Pin: ", this->spi_conf_.mosi);
  LOG_PIN("  MISO Pin: ", this->spi_conf_.miso);
  LOG_PIN("  CLK Pin:  ", this->spi_conf_.clk);
  LOG_PIN("  CS Pin:   ", this->spi_conf_.cs);
  LOG_PIN("  GDO0 Pin: ", this->spi_conf_.gdo0);
  LOG_PIN("  GDO2 Pin: ", this->spi_conf_.gdo2);
}

}  // namespace wmbusgw
}  // namespace esphome