# 修订记录：

## 2021-6-2：

1、修改了03-D3D12PlacedTexture示例中的代码，删除了d3dx12.h头文件，并且把所有引用自该文件中的类、方法等都改回了最原始的d3d12中的结构体和函数的引用，这主要是因为现在VS2019中的C++编译器标准已经升级，不在支持那种嵌入的从类中返回临时对象并取其地址作为参数的情况，代码编译中会报错，所以干脆把所有这些引用全部删除，后续的项目也会逐步的删除关于d3dx12.h所有引用。1、2示例中之前就已经去掉了，当时是为了保证大家都能接触到最原汁原味的D3D12结构体、函数、接口等。

2、修改了15-CPUThreadsBarrier示例的窗口标题。

3、现在的DXGI在升级之后，当刚创建完时，就使用MakeWindowAssociation函数关闭Alt+Enter键转全屏时，就会引发一个DXGI的警告，它会抱怨说还不知道窗口是哪个，所以将这个调用调整到了创建交换链完成之后，这时候实质上DXGI就知道了我们操作的窗口是哪一个。

4、现在除了多显卡渲染的几个例子之外，其它所有的例子都会频繁报：

```sh
D3D12 ERROR: ID3D12CommandQueue::Present: Resource state (0x800: D3D12_RESOURCE_STATE_COPY_SOURCE) (promoted from COMMON state) of resource (0x000001B737E63E00:'Unnamed ID3D12Resource Object') (subresource: 0) must be in COMMON state when transitioning to use in a different Command List type, because resource state on previous Command List type : D3D12_COMMAND_LIST_TYPE_COPY, is actually incompatible and different from that on the next Command List type : D3D12_COMMAND_LIST_TYPE_DIRECT. [ RESOURCE_MANIPULATION ERROR #990: RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE]

```

这个错误，这是Windows内部的问题，跟我们的例子无关，我目前分析大概的原因是因为像我这个笔记本因为有两个显卡，而显示器又默认连接在集显上，可是我们的例子都是在独显上渲染的，这样就会有一个最终隐藏的需要将渲染结果从独显复制到核显上的过程，而Windows内部貌似现在没有处理好这个资源状态的切换，所以就会频繁报这个#990的错误，但是当我们自己控制多个显卡的渲染，并控制共享纹理和渲染目标资源状态（资源屏障同步）时，或系统中只有一个显卡，并且显示器直接接在这个唯一显卡上时都不会报这个错误。这个目前我还没有找到修正的方法，或许只能等到Windows更新D3D12相关的模块了。但这个问题不影响例子的运行，所以现在就先忽略吧。

5、修正了Readme文件中图片显示异常的问题。
