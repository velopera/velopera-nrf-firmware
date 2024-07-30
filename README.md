# velopera-nrf-firmware

The nRF9160 SICA is based on Zephyr, the development environment is based upon nRF Connect SDK, integrated into Visual Studio Code.

Project URL: [https://github.com/velopera/velopera-nrf-firmware](https://github.com/velopera/velopera-nrf-firmware)

1. ## Installing the required development environment and downloading the example project

After installing and starting the [nRF Connect Desktop](https://docs.google.com/document/d/1RdgDjMkioEuY_dKlOK0IRWg318wvAs84/edit#heading=h.1hmsyys), install the Toolchain Manager app. Open Toolchain Manager app and install the latest nRF Connect SDK. After installing the latest SDK [Visual Studio Code](https://docs.google.com/document/d/1RdgDjMkioEuY_dKlOK0IRWg318wvAs84/edit#heading=h.1hmsyys). Open Visual Studio Code and install [nRF Connect for VS Code Extension](https://docs.google.com/document/d/1RdgDjMkioEuY_dKlOK0IRWg318wvAs84/edit#heading=h.1hmsyys). Download the [example project](https://docs.google.com/document/d/1RdgDjMkioEuY_dKlOK0IRWg318wvAs84/edit#heading=h.1hmsyys).

After installing the required development environment and downloading the example project we are ready to build and run the example project.

2. ## How to build and run the example project:

To build and run the example project follow these steps.

1. Open Visual Studio Code and open nRF Connect for VS Code Extension.
2. Hit the “Open an existing application” button on the welcome page and browse the directory of the example project.
3. After opening the example project make sure that it's active on the “APPLICATIONS” section. If it's not active simply click on the project name.
4. After activating the example project click the “Build” button on the “Actions” section.
5. Once building is finished click the “Flash” button on the “Actions” section.

The example project has a modular structure, where each module has a defined scope of responsibility. The communication between modules is handled by the Zbus using messages that are passed over channels. The diagram is shown in Figure 16 visualizes the relationship between applications and channels.

3. ## Software dependencies:

- [Install nRF Connect Desktop](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-desktop)

https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-desktop

- [Install Visual Studio Code](https://code.visualstudio.com/)

https://code.visualstudio.com/

- [Install nRF Connect for VS Code Extension](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-VS-Code/Download#infotabs)

https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-VS-Code/Download#infotabs

![](https://lh7-rt.googleusercontent.com/docsz/AD_4nXdaKbThNMQf_KvXwoSr5z2xjfna3FCxUAE8J3zxRi-8LpAcn5BpLV43t4Ulf8NH462nay11zXsmnVphfjWxvixvqEXJq9-UAiuepCxMXOJkY6YVaQJs9wGl9FnSHzyaP6aNviVIyhFWIYtww6eft-MypxwkL6EztsR72EnJ-ylTeDZ_D_6MP8k?key=WvO7TA-YR9oKEdgVZ0dRKQ)1. ### Network

The “Network” application connects to LTE via the Zephyr NET stack. Once connection is done the network application publishes network status to the “Network Status Channel” via Zbus. Network application also has a “Power Saving Mode (PSM)” feature. LTE goes to PSM if there is not any network activity in “1 minute (TODO: to be defined)”.

2. ### Security

The example project supports Transport Layer Security with Pre Shared Keys (TLS-PSK). Once the “NETWORK_CONNECTED” message comes from “Network Status Channel” the “TLS-PSK” application fetches International Mobile Equipment Identity (IMEI) from LTE modem to register as PSK-ID and then it registers the PSK.

3. ### UART Transceiver

The example project uses INT-COM pins of the board to communicate with the Nina module. The “UART Transceiver” application receives CAN and Compass data as a JSON object via UART interrupt handler. An example of received message is shown in Figure 17.

Figure 17. Received message example

Application publishes received messages to the “Payload Channel” via Zbus.

4. ### MQTT

The “MQTT Uplink” application listens to messages from “Network Status Channel” and “Payload Channel”. Once the “NETWORK_CONNECTED” message comes from “Network Status Channel”, the MQTT Uplink application attempts to connect to the broker via the function of “Dynsec MQTT Helper”. After a successful connection, the MQTT Uplink application waits for the messages from the “Payload Channel” to publish received UART messages to the broker.

The sequence diagram is shown in Figure 18 visualises the most significant chain of events during operation of the example project:

![](https://lh7-rt.googleusercontent.com/docsz/AD_4nXdnl0Vdg3JoChi3CLvgBmdb-Sp2-5TZeR3fpCEMyGnDLU8AuHp8s4XU-myFPVhTvzilA8ygCAj_DVfFm-KkHjddHFWk4kJgfrBqcpKlTnS1s2-59ltuxuKR8BKJguiHm-hsguoWuoJIbBl1bQs6xxTPyEwKY_xw2wqoWiTBvwTgLV1TciHTRyw?key=WvO7TA-YR9oKEdgVZ0dRKQ)5. ### Testing

After programming the example project to the nRF9160 module, complete the following steps to test it:

1. Connect the RX pin of the USB-TTL converter to the SDA pin of the extension header.
2. Connect the TX pin of the USB-TTL converter to the SCL pin of the extension header.
3. Connect the GND pin of the USB-TTL converter to the GND pin of the extension header.
4. Connect the USB-TTL converter to the computer. Open a terminal emulator and select the USB port to see debug output.

The following UART debug output should be displayed in the terminal emulator:

TODO: add debug output.

6. ### Useful Links

- ### [Zephyr OS documentation](https://docs.zephyrproject.org/latest/index.html)

https://docs.zephyrproject.org/latest/index.html

- ### [nRF Connect SDK documentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html)

https://developer.nordicsemi.com/nRF\_Connect\_SDK/doc/latest/nrf/index.html

- ### [Nordic DevAcademy courses](https://academy.nordicsemi.com/)

https://academy.nordicsemi.com/

- ### [nRF9160 AT commands](https://infocenter.nordicsemi.com/topic/ref_at_commands/REF/at_commands/intro.html?cp=2_1)

https://infocenter.nordicsemi.com/topic/ref\_at\_commands/REF/at\_commands/intro.html?cp=2\_1

- ### [nRF9160 SiP modem firmware](https://www.nordicsemi.com/Products/nRF9160/Download#infotabs:~:text=nRF9160%20SiP%20modem%20firmware%C2%A0,-Programming%20app%20available)

https://www.nordicsemi.com/Products/nRF9160/Download#infotabs:\~:text=nRF9160%20SiP%20modem%20firmware%C2%A0,-Programming%20app%20available
