# Kinect Server on Windows - Color, Depth, Infrared, Body Tracking, Face Tracking

1) git clone https://github.com/sdebnathusc/kinect_bridge_windows.git OR Download zipped version from GitHub and unzip it in local

2) Say, the location is `kinect_bridge_windows`

3) `cd kinect_bridge_windows` and then create a new folder name `build`

4) Open CMake and set the location for Source to `kinect_bridge_windows` and Build to `kinect_bridge_windows\build`

![](https://github.com/sdebnathusc/kinect_bridge_windows/blob/master/images/1.png)

5) Next Click on `Configure`, and select `Visual Studio 14 2015` and press Finish. After that, click on `Generate` to generate the visual studio solution (.sln) file in the build folder. 

![](https://github.com/sdebnathusc/kinect_bridge_windows/blob/master/images/2.png)

6) Go to the `build` folder and click the .sln file generated to open it in Visual Studio. Build the entire solution. Set Kinect_server as the startup project by right clicking on it and selecting `Set as StartUp Project`. And then run the Kinect_server by clicking F5. This starts Kinect Server. 
 
