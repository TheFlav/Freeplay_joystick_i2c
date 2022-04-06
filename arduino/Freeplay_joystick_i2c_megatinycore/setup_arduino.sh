curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
bin/arduino-cli config init
bin/arduino-cli config add board_manager.additional_urls http://drazzy.com/package_drazzy.com_index.json
bin/arduino-cli core update-index --additional-urls http://drazzy.com/package_drazzy.com_index.json
bin/arduino-cli core search megaTinyCore
bin/arduino-cli core install megaTinyCore:megaavr

