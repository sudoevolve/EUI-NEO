# 鏁版嵁宸ュ叿鍙樻洿楠岃瘉

鏈疆鎸夊姛鑳藉湪 dev 鍒嗘敮鍒嗗埆鎻愪氦锛氳繃鏈熷紓姝ュ洖璋冦€佷覆鍙ｅけ璐ュ洖婊氬拰鐙崰鎵€鏈夋潈銆侀敭鐩樹笌璇箟銆佹挙閿€棰勭畻銆佽櫄鎷熻〃鏍笺€佷繚瀛樹笌鐩綍閫夋嫨銆佸疄鏃舵洸绾裤€傛病鏈夊紩鍏ュ閮ㄤ緷璧栵紝澶嶇敤鐜版湁 CMake銆佽櫄鎷熷垪琛ㄣ€佽緭鍏ョ姸鎬佸強娓叉煋鎵规銆?

## 澶嶇幇鐜涓庢柟娉?

- Windows 11锛?0.0.26300锛夛紝Intel Core Ultra 5 125H锛孖ntel Arc Graphics锛岀墿鐞嗗唴瀛?25,263,726,592 瀛楄妭銆?
- Visual Studio 18 2026 / MSVC 19.51.36247锛孯elease x64锛堥」鐩厤缃?`/O1 /Ob2 /MD`锛夛紝GLFW + OpenGL銆?
- `linechart_realtime --benchmark` 姣旇緝鍚屼竴浜岃繘鍒剁殑鍘熷睍绀鸿矾寰勪笌瀹炴椂璺緞锛屾瘡绉嶉鐑?3 娆★紝閲囨牱 10 娆★紱璁℃椂瑕嗙洊 compose + layout锛屼笉鍖呭惈 GPU 鎴?FPS銆?
- 鍥惧 356銆侀珮 312锛?,000 涓噰鏍凤紱瀹炴椂璺緞姣忔閫掑 revision锛屽寘鍚噸鏂伴檷閲囨牱銆傚睍绀鸿矾寰勪繚鐣欏師鏈夐€愮偣鏍囩涓?Tooltip锛屽疄鏃惰矾寰勭渷鐣ュ畠浠紝鍥犳涓よ€呬骇鍝佽兘鍔涙湁宸埆锛屾祴閲忚鏄庡ぇ鏁版嵁瀹炴椂妯″紡鐨勪唬浠凤紝涓嶄唬琛ㄧ瓑鍔熻兘鐨勯€氱敤鍔犻€熷€嶆暟銆?
- 鎾ら攢姣旇緝鍚屼竴 InputModel 瀵?1 MiB 鏂囨湰淇濆瓨 128 娆＄紪杈戯細浠呮繁搴﹂檺鍒讹紙棰勭畻鏈€澶у€硷級涓庨粯璁?8 MiB 棰勭畻銆傛寚鏍囨槸蹇収瀵硅薄鍜屽瓧绗︿覆瀹归噺璁¤垂锛岄潪杩涚▼宄板€煎唴瀛樸€?

## 琛屼负瑕嗙洊

- async_tasks锛氬畬鎴愬悗閲嶅惎/鍙栨秷銆乫orget 鍚庡鐢?key銆佸悓鎵规鍥炶皟鍙栨秷鍚庣画鍥炶皟銆?
- serial_port锛氫笉鍙鍒躲€佸彲绉诲姩銆佹棤鏁堣澶囧拰閰嶇疆澶辫触锛汸OSIX PTY 鍒嗘敮闇€瑕?Linux/macOS 鐜杩愯銆俉indows 鏃犵湡瀹炰覆鍙ｇ‖浠堕獙鏀躲€?
- accessibility_probe锛氱湡瀹炵獥鍙ｃ€乀ab/Shift+Tab銆佺鐢ㄨ烦杩囥€丒nter/Space銆乻lider銆佺粍鏂瑰悜閿€乨ropdown銆佹ā鎬佺害鏉熷強鎭㈠銆佺劍鐐规甯冨眬銆?
- datatable锛氬崄涓囪鑺傜偣涓婇檺銆佺ǔ瀹氭暟鍊兼帓搴忋€佺己澶卞垪銆佺瓫閫変笌 revision銆佹粴鍔ㄥ亸绉汇€佸垪瀹介敭鐩樿皟鏁淬€佹帓搴忓悗鐐瑰嚮鍘熷绱㈠紩銆?
- platform_dialog锛歐indows 鍘熺敓鎵撳紑/淇濆瓨/鐩綍瀵硅瘽妗嗚嚜鍔ㄥ彇娑堛€佽秴闀块粯璁ゆ枃浠跺悕澶辫触锛汱inux stub 瑕嗙洊鍙傛暟鍙婇敊璇緭鍑恒€俉indows 鐨勪繚瀛橀€変腑/瑕嗙洊纭鐢变氦浜?fixture 楠岃瘉鍏ュ彛鎻愪緵锛岃嚜鍔ㄦ祴璇曚笉鍐欑敤鎴锋枃浠躲€?
- input_history_budget锛氶绠楁窐姹般€? 棰勭畻銆佽秴澶у揩鐓с€乽ndo/redo銆乁TF-8銆?
- linechart_realtime锛氬嘲璋蜂繚鐣欍€佽緭鍑轰笂闄愩€佹柇鐐广€佺┖鏁版嵁銆佸尯闂村拰鏁板€艰寖鍥淬€佽妭鐐规暟鍙婃弿杈瑰懡涓?杈圭晫銆?
- polyline_probe锛氬疄闄呭悗绔粯鍒朵笌棰滆壊鍒锋柊锛汷penGL 璇诲儚绱犻獙璇佺嚎娈点€佹柇鐐广€俈ulkan 杩愯鍚屼竴娓叉煋璺緞锛屽皻鏈鍔?framebuffer 璇诲洖鏂█銆?

## 浜や簰鍏ュ彛

`data_tools_viewer` 灞曠ず鍗佷竾琛岃〃鏍间笌瀹炴椂鏇茬嚎銆佺瓫閫夈€佽〃澶存帓搴忋€佸垪瀹芥嫋鍔ㄣ€佷繚瀛樸€佺洰褰曢€夋嫨鍜岄暱鏂囨湰缂栬緫銆傞敭鐩樻搷浣滆鏄庤 [鏃犻殰纰嶆枃妗(../../guides/keyboard-accessibility.md)銆?

`data_tools_probe` 鑷姩杈撳叆 `Sensor 3`锛屾柇瑷€鍗佷竾琛岀瓫閫夊悗涓?6,250 琛屻€佽緭鍏ユ浠嶆湁鐒︾偣涓斿疄鏃舵洸绾垮瓨鍦ㄣ€侽penGL 鍙繍琛?`data_tools_probe output.ppm` 瀵煎嚭褰撳墠缁勫悎鎴浘锛涢粯璁?CTest 涓嶅啓鎴浘銆傚疄闄呮鏌ヤ簡 GLFW + OpenGL 瀵煎嚭鍥句腑鐨勫垪瀵归綈銆佽緭鍏ョ劍鐐广€佹洸绾垮嘲璋峰拰鏂囨湰甯冨眬銆?

## 鍚屾満娴嬮噺鍘熷鏁版嵁

2026-11-12锛屽洓缁勫悎缂栬瘧鍜屾祴璇曞畬鎴愬悗娴嬮噺锛岄伩鍏嶅苟鍙戞瀯寤鸿礋杞姐€傚鐜帮細

```powershell
cmake --build build-dev-glfw-opengl --config Release --target linechart_realtime
./build-dev-glfw-opengl/Release/linechart_realtime.exe --benchmark
```

CSV 鍓嶅叚鍒椾负鍦烘櫙銆佹ā寮忋€侀噰鏍疯妯°€佸簭鍙枫€乁I 鑺傜偣鏁般€乧ompose+layout 寰锛?

```csv
chart,legacy,5000,0,35006,653980
chart,legacy,5000,1,35006,710688
chart,legacy,5000,2,35006,832449
chart,legacy,5000,3,35006,775891
chart,legacy,5000,4,35006,819235
chart,legacy,5000,5,35006,792573
chart,legacy,5000,6,35006,785122
chart,legacy,5000,7,35006,792484
chart,legacy,5000,8,35006,814662
chart,legacy,5000,9,35006,810646
chart,realtime,5000,0,9,19.7
chart,realtime,5000,1,9,20.2
chart,realtime,5000,2,9,19.8
chart,realtime,5000,3,9,27.2
chart,realtime,5000,4,9,19.5
chart,realtime,5000,5,9,19.4
chart,realtime,5000,6,9,19.3
chart,realtime,5000,7,9,19.2
chart,realtime,5000,8,9,19
chart,realtime,5000,9,9,19.2
undo,18446744073709551615,128,134225920
undo,8388608,7,7340480
```

涓綅鏁帮細灞曠ず璺緞 792.529 ms锛屽疄鏃惰矾寰?0.01945 ms锛涜妭鐐规暟 35,006 鈫?9銆備袱鏉¤矾寰勭殑 UI 鍔熻兘涓嶅悓锛屼笉鑳藉皢姝ゅ€煎綋浣滃悓绛夊姛鑳界粯鍥炬垨 GPU 鍔犻€熷€嶆暟銆傛挙閿€鏁版嵁鏈€鍚庝笁鍒楁槸棰勭畻銆佷繚鐣欏揩鐓ф暟銆佽璐瑰瓧鑺傛暟锛氫粎闄愬埗娣卞害涓?134,225,920 瀛楄妭锛? MiB 棰勭畻涓?7,340,480 瀛楄妭銆傛湭娴嬮噺 GPU 鏃堕棿銆丷SS 宄板€兼垨 FPS銆?

## 鍚庣楠岃瘉

鍥涗釜鐙珛鐩綍閮藉畬鎴愰厤缃€佸畬鏁?Release 鏋勫缓锛堝惈 fixtures锛夈€丆Test锛?

| 缁勫悎 | 娴嬭瘯缁撴灉 |
| --- | --- |
| GLFW + OpenGL | 46 / 46 閫氳繃 |
| SDL2 + OpenGL | 46 / 46 閫氳繃 |
| GLFW + Vulkan | 45 閫氳繃锛? 璺宠繃锛? 澶辫触 |
| SDL2 + Vulkan | 45 閫氳繃锛? 璺宠繃锛? 澶辫触 |

Vulkan 璺宠繃鐨勬槸宸叉湁 `markdown_viewport_probe`锛屽叾鍍忕礌璇诲洖楠岃瘉鍙敮鎸?OpenGL銆傛柊鍔犵殑 accessibility銆乨ata_tools銆乸olyline 鎺㈤拡鍦ㄥ洓缁勫悎鍧囬€氳繃銆係DL2 鍒濇鍥炲綊鏆撮湶鍒濆鍖栧墠 `SDL_PushEvent` 鐨?timer 绔炰簤锛涘悗绔幇鍦ㄤ粎鍦?SDL 浜嬩欢瀛愮郴缁熸湁鏁堟椂鍙戦€佸敜閱掍簨浠讹紝`async_tasks` 鍦?SDL2 + Vulkan 杩炵画杩愯 30 娆￠€氳繃銆?

鏈€缁堝鏌ヤ慨姝ｄ簡鍒濆鍒楀浣庝簬鏈€灏忓€笺€佽〃澶翠笌铏氭嫙鍒楄〃 scrollbar gap 涓嶄竴鑷淬€佸脊灞傚唴 Escape 鎭㈠鐒︾偣鍜屾棩鏈?鏃堕棿鍒楅敭鐩樿皟鏁淬€傛渶鍚庣殑鍒楀瀵归綈淇鍚庯紝鍥涚粍鍚堝啀娆″畬鎴愬畬鏁?Release 鏋勫缓锛屽苟鍒嗗埆琛ヨ窇 `datatable` 涓?`data_tools_probe`锛屽潎涓?2 / 2 閫氳繃銆俙git diff --check` 閫氳繃銆傚叡浜暟鎹殑 revision銆佺嚎绋嬬害鏉熶笌瀹炴椂妯″紡鐨勫睍绀哄彇鑸嶅潎宸插湪缁勪欢鏂囨。璇存槑銆?

鏈満鏈繍琛?Linux/macOS 鍘熺敓鍒嗘敮銆佺湡瀹炰覆鍙ｇ‖浠躲€佺郴缁熷睆骞曢槄璇诲櫒鎴?Vulkan 鏇茬嚎鍍忕礌璇诲洖锛涘悗涓ら」涓嶅湪鏈樁娈靛疄鐜拌寖鍥淬€俉indows 瀵硅瘽妗嗚嚜鍔ㄩ獙璇佸彇娑堜笌閿欒璺緞锛岄€変腑璺緞鍜岃鐩栫‘璁や粛闇€浜や簰楠屾敹銆?
