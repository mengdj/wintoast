# wintoast

��ҵ����Ŀ��ҪŪ��windows��toast���ѣ�ĳ�������˰���û�ҵ��������Լ�Ūһ�������ܱȽϼ򵥣���ϲ������ñȽϼ򵥣���3��
1.��ʼ��
```c++
xmstudio::TOAST_CFG cfg = { 0 };
cfg.hinstance = hInstance;
cfg.font.width = 0;
cfg.font.color = RGB(255, 255, 255);
cfg.font.height = 14;
cfg.width = 250;
cfg.height = 40;
cfg.font.name = TEXT("����");
cfg.background.alpha = 1000;
cfg.background.color = RGB(0, 122, 204);
cfg.background.translate_color = 0;
cfg.background.translate_flags = LWA_ALPHA;
xmstudio::toast::init(cfg);
```
2.����
```c++
xmstudio::toast::show(hWnd, TEXT("ͣ��1.5��(����1)"), 1500, 0, 0);
```
3.�ǵ�Ҫ�ͷ���ԴŶ
```c++
xmstudio::toast::destory();
```

<img src="https://raw.githubusercontent.com/mengdj/wintoast/master/Release/GIF.gif" width="75%"/>


