# FlowMap 水面流动效果演示（SDL3 + C++）

这是一个用C++和SDL3实现的FlowMap水面/纹理流动效果演示程序。

## 效果原理

FlowMap贴图的R、G通道分别存储了一个二维流向量的 U、V 分量（值域 [0,255]，使用时映射到 [-1,1]）。

渲染时，对屏幕每个像素执行以下步骤：
1. 采样 FlowMap，得到当前位置的流动向量v。
2. 构造两个时间相位，相差半个周期：
  - phase_a = frac(time)
  - phase_b = frac(time + 0.5)
3. 用流动向量v偏移基础纹理的UV，并采样像素点得到两次采样 color_a、color_b。
4. 用时间混合因子 blend = abs(frac(time) * 2 - 1) 对两次采样做线性插值。

简单的理解为：FlowMap是一张法线贴图，其中的每个像素(Red and green component)是一个二维向量，
顺着向量的方向采样单位步长*time的像素，就可以形成扭曲动画，将这个扭曲动画偏移一半的周期然后叠加在一起，以形成循环动画。

## Build

1, Install vcpkg and SDL3 library.  
2, Run following command to build it.

```Bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```