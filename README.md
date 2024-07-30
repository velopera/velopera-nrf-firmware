# velopera-nrf-firmware

## VELOpera nRF Firmware

\*\*The nRF9160 SICA is based on Zephyr, the development environment is based upon nRF Connect SDK, integrated into Visual Studio Code.

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

The velopera-nrf-firmware has a modular structure, where each module has a defined scope of responsibility. The communication between modules is handled by the Zbus using messages that are passed over channels. The following diagram visualises the relationship between applications and channels.

![](https://lh7-rt.googleusercontent.com/docsz/AD_4nXfFjEh5RzanGolUuawD_KVWFtONa_ud3pm_vzRpMRjX8qRm6PNMX_cjNiM_NdHVp_nhhvZwrehWSMTSCRy_zQIbmUJ6pLTGKUO1RRhKaVXQ2B-uVfIOtkNvMV9poVfYfkj-Wx-sdNBrnkxQaQqngGRQVIi1?key=9pb681neQGEwGwsz9r7_bg)

### Network

The “Network” application connects to LTE via the Zephyr NET stack. Once connection is done the network application publishes network status to the “Network Channel” via Zbus. Network application also has a “Power Saving Mode (PSM)” feature. LTE goes to PSM if there is not any network activity in 1 minute.

### Security

The example project supports Transport Layer Security with Pre Shared Keys (TLS-PSK). Once the “NETWORK_CONNECTED” message comes from “Network Channel” the “TLS-PSK” application fetches International Mobile Equipment Identity (IMEI) from LTE modem to register as PSK-ID and then it registers the PSK.

### UART Transceiver

The example project uses INT-COM pins of the board to communicate with the Nina module. The “UART Transceiver” application receives CAN and Compass data as a JSON object via UART interrupt handler. E.g.

Application publishes received messages to the “MQTT Channel” via Zbus.

### MQTT

The “MQTT Uplink” application listens to messages from “Network Channel” and “MQTT Channel”. Once the “NETWORK_CONNECTED” message comes from “Network Channel”, the MQTT Uplink application attempts to connect to the broker via the function of “Dynsec MQTT Helper”. After a successful connection, the MQTT Uplink application waits for the messages from the “MQTT Channel” to publish received UART messages to the broker.

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
