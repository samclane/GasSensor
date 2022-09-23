# GasSensor

GasSensor using [Wio Terminal](https://www.seeedstudio.com/Wio-Terminal-p-4509.html) and [4-Channel Gas Sensor Module](https://wiki.seeedstudio.com/Grove-Multichannel-Gas-Sensor-V2/).

Also uses [Edge Impulse](https://studio.edgeimpulse.com/public/65919/latest) for gas classification.

# Libraries Used

I didn't use the PlatformIO installer, as I found the libraries were out of date. Instead, I manually installed them into the `lib` folder.

Throw these in the /lib folder

- [Seed Arduino LCD](https://github.com/Seeed-Studio/Seeed_Arduino_LCD)
- [Seeed Arduino MultiGas](https://github.com/Seeed-Studio/Seeed_Arduino_MultiGas)
- [Gas Classifier](https://studio.edgeimpulse.com/public/65919/latest/deployment)
  - To build, select "Arduino Library" at the top, the "Build" at the bottom. Extract the resulting folder to your `/lib` folder


# Simple Chassis

Not required, but I made a 3D printable chassis for the Wio Terminal and the Gas Sensor Module.

[Chassis](https://thangs.com/designer/samclane/3d-model/Wio%20Terminal%20%2B%20Battery%20Pack%20Project%20case-362594?manualModelView=true)

![](https://storage.googleapis.com/gcp-and-physna.appspot.com/uploads/attachments/82364099-cf15-429a-8ba1-ac3171f2dec7/IMG_20220923_051937.jpg?GoogleAccessId=gcp-and-physna%40appspot.gserviceaccount.com&Expires=1664450787&Signature=CYny%2FneD4KO%2F80gqKjv2GlEpy1ObrZKMfYGWunPYXqz6sHm5SSvMUqq%2FOmQFhXepo5CKAEOwl%2BGCtyE1DR3wuFyXPPYAvBjJnxF2D0UsMKXXca2xl8xfmVa%2BuZtqIOMWWOwDlshHNLTu4Ki4JxFRsBep4o9m5T2MZ5PeCaLY2ISsB4OJ3M0VwUD2094YCEieKGZMkYtZrgAnlOIJPpBaiDjo%2F7tSPvGWF0OO%2Bm6AqD37gcPd%2BPsN%2BKHf%2FL9K9n66zgdaU0plmNvb2azjiVv0Njp4qLZzvXnv8OrSCWYQCQ5EmOtnKezf82sxRlNmVV3A%2FMPUYuQrqHLsk2njB0jQYQ%3D%3D)