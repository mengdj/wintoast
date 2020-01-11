# wintoast

因业务项目需要弄个windows的toast提醒，某网搜索了半天没找到，索性自己弄一个，功能比较简单，不喜勿喷调用比较简单，就3步
1.初始化
```c++
xmstudio::TOAST_CFG cfg = { 0 };
cfg.hinstance = hInstance;
cfg.font.width = 0;
cfg.font.color = RGB(255, 255, 255);
cfg.font.height = 14;
cfg.width = 250;
cfg.height = 40;
cfg.font.name = TEXT("宋体");
cfg.background.alpha = 1000;
cfg.background.color = RGB(0, 122, 204);
cfg.background.translate_color = 0;
cfg.background.translate_flags = LWA_ALPHA;
xmstudio::toast::init(cfg);
```
2.调用
```c++
xmstudio::toast::show(hWnd, TEXT("停留1.5秒(窗口1)"), 1500, 0, 0);
```
3.记得要释放资源哦
```c++
xmstudio::toast::destory();
```

<img src="https://raw.githubusercontent.com/mengdj/wintoast/master/Release/GIF.gif" width="75%"/>
