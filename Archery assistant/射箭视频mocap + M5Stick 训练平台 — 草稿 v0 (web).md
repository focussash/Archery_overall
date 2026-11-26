# 射箭视频mocap + M5Stick 训练平台 — 草稿

Nov 14, 2026 · @Zijie

## 一句话结论

这份文件是交给 Claude Code 的大方向 brief。先搭一个能跑、能看的粗糙原型，再一步步 refine，必要时 refactor。路线是：**一台侧面相机（拍到的是冠状面 frontal plane）+ 手机本地跑姿态估计 (on-device pose estimation)**。拉弓中的实时反馈在手机上完成，箭后复盘和重分析可以走云。出箭时刻 (release) 将来交给 M5Stick 的 IMU 来判，但接口暂不定。

有三条硬约束，Claude Code 从第一天就得当真：

1. **拉弓中的实时反馈只能在手机本地跑，不能走云。** 你说"要 GPU 就走云端"，这对箭后复盘没问题；但满弓 (full draw) 到撒放 (release) 只有一两秒，上传视频、云端推理、再把结果传回来让 M5Stick 震动，延迟和抖动都不可控。所以是两条链路：实时链路 (real-time path) 全在手机上，离线链路 (offline path) 可以上云。
2. **非丁非八是开放式站姿 (open stance)，躯干不平行于相机成像平面。** 拉距、肩宽这些 2D 量会被透视缩短 (foreshortening)。只做自己跟自己比的话问题不大，前提是每次相机位置一致：要有一套相机摆放流程 (camera placement protocol)，指标也尽量用比值或角度，少用绝对长度。将来要给别人用，这条会变得更重要。
3. **靠位 (anchor) 不能当成切阶段的必要条件。** 空拉过耳的人没有触脸靠位，所以满弓或持弓阶段 (full draw / hold) 要用拉弓手速度趋近 0 来定义。靠位相关的指标做成可选模块，不要写死在流程里。

## 已定的事（2026-11-15）

| 项 | 已定 | 对设计的影响 |
| --- | --- | --- |
| 相机 | 1 台，放侧面三脚架（射线旁、正对胸前），拍冠状面 | 第二台相机写进路线图，但不进第一版 |
| 运行平台 | 手机（你的 Samsung，即 Android）；要 GPU 的部分走云，结果回到笔记本或网页 | 实时链路纯本地；云只管离线 |
| 反馈时机 | 两种都要：拉弓中的同步反馈 (concurrent feedback)；箭后几秒的终末反馈 (terminal feedback)，以收势 follow-through 为主 | 两套不同的规则和延迟预算 |
| 反馈通道 | 振动为主（大概率走 M5Stick），耳机待定，具体再议 | 反馈做成可插拔的输出接口 (pluggable output)，第一版先用手机自己的振动或提示音当占位 |
| 指标 | 越多越好；收势 (follow-through) 优先级高；靠位可选 | 指标做成注册制的插件 (metric registry)，加一个指标不用改流程 |
| 站姿 | 开放式，高颖“非丁非八” | 见硬约束 2 |
| 用户 | 先自己用，将来给别人用；同一时间只拍一个人 | 不做多人检测；个人参数（身高、左右手、有无靠位）做成配置文件 (profile) |
| M5Stick | 两个项目并行开发 (co-develop)，接口暂不定 | 只留接口（事件时间戳输入、反馈输出），不写具体协议 |

## 架构草图 (architecture)

&#91;embedded content: 架构草图 · 实时链路 + 箭后链路\]

上面一排是实时链路，全部在手机上跑，延迟预算要在满弓窗口以内；下面一排是箭后链路，可以慢。M5Stick 用虚线画，表示只留接口、协议待定。每个框在代码里应该是一个可以单独替换的模块，比如姿态模型从 MediaPipe 换成 RTMPose，不该影响阶段识别和指标。

## 先纠一个概念：相机放哪，看到的是哪个平面

射箭时你是侧对靶子站的，箭走的是你两肩连线的方向（内外侧, mediolateral）。所以开弓 (draw)、靠位 (anchor)、撒放 (release) 这些动作，主要发生在你自己的**冠状面 (frontal / coronal plane) + 横断面 (transverse plane)**，而不是矢状面 (sagittal plane)。你口头说的"侧面拍一台"，如果指的是相机站在射线旁边、正对你胸口，那在解剖学上拍到的是**冠状面**。平面叫什么不重要，Sports2D 写明了动作只要在矢状面或冠状面里都行，关键是要确认你想看的东西在不在这个平面里。

| 相机位置 | 拍到的平面 | 看得清 | 看不清 | 安全 |
| --- | --- | --- | --- | --- |
| 射线旁、正对你胸前 | 冠状面 | 持弓臂 (bow arm) 高度、两肩高低差 (shoulder height difference)、头的倾斜 (head tilt)、拉距 (draw length，这里看水平位移)、靠位时手离嘴的距离、撒放后手往哪走 (follow-through) | 拉弓肘 (draw elbow) 在不在箭线后面、躯干转动 (trunk rotation)、持弓肩有没有前顶 (bow-shoulder protraction / elevation)（这些都是深度） | 安全 |
| 头顶往下拍 | 横断面 | line / alignment（肘、肩、箭是不是一条线）、躯干转动 | 高度信息 | 安全，但不好架 |
| 你身后、顺着箭的方向拍 | 接近矢状面 | 弓的倾斜（cant），拉弓肘相对箭线的前后、高低（也能看 line，而且比头顶好架） | 拉距（这时变成深度） | 安全 |
| 从靶子那边往回拍 | 矢状面 | — | — | **绝对不行，在射线上** |

有两件汉弓特有的事情要先有心理准备：

- 你用扳指 (thumb ring)、拇指勾弦 (thumb draw)。现成的手部关键点 (hand keypoints) 模型基本是拿普通手势训练的，拇指被扳指挡住、又贴在脸旁边的时候准不准，得实测才知道。
- 你的靠位是羽毛贴嘴唇，用脸部关键点 (face keypoints) 里的嘴角 (mouth corner)去定义"靠位是否一致"，在正面相机里是看得到的。

## 市面上已有的东西

商业产品基本都是"弓上或者手腕上一个 IMU + 手机 app"，核心功能就是把一箭切成几个阶段、各自计时、再做箭与箭之间的对比。视频这边有一个刚出来的多相机 app，但不支持传统弓。

| 东西 | 形态 | 测什么 | 对你的参考价值 |
| --- | --- | --- | --- |
| [Mantis X8](https://www.americanhunter.org/content/first-look-mantis-x8-archery-trainer) | 装在弓上的 IMU，蓝牙连 app | 整箭的弓体轨迹，按 USA Archery NTS 分阶段着色、计时；还有 cant（弓的左右倾斜）和稳定分数 (stability score) | 阶段划分 + 计时 + 每箭对比，可以直接当你 M5Stick 那边的功能模板。汉弓没地方装 |
| [Steady-Aim A1](https://lancasterarchery.com/products/steady-aim-a1-shooting-analysis-system) | 装在瞄具杆上 | steadiness/drift 分数、hold time、振动分析 | 同上，需要瞄具 |
| [Garmin ArcheryX](https://the5krunner.com/2026/08/25/garmin-archeryx-explained/) | Fenix 9 手表（加速度计 + 陀螺仪） | 出箭前的持弓臂晃动、MOA 稳定度、瞄准轨迹、执行时间、计箭数 | **最接近你的路线**：传感器戴身上，不装弓上。但官方只支持复合弓，出箭怎么检测也没公开 |
| OnForm 等视频 app | 手机录像 + 画线 | 人工看 | 就是你要自动化掉的那一步（[Bow International](https://bow-international.com/?p=7127) 提到过） |
| [ShotLab + BowPose](https://huggingface.co/jotux/BowPose) | 多相机射箭姿态分析 app，外加一个弓的关键点模型 | 人体姿态 (pose) + 弓（弓把 riser、弓梢 limb tips、稳定杆 stabilizer） | 2026 年 11 月刚发的，CC-BY-4.0，完全用合成图训练，只支持反曲、光弓和复合弓，真实视频上的精度还没测过。汉弓大概率要自己重新训练，但他们"用 Blender 合成数据训练"的思路可以直接照搬 |

学术界有两篇可以借鉴：

- [Lee et al. 2024](https://mdpi.com/2076-3417/14/24/11838)：用正面的转播画面、MediaPipe。靠位开始的判据是"右臂角度 10 帧内平均变化 < 1°"；出箭时刻靠在脸部框里用 Hough 变换找弓弦。最后跟 Dartfish 专家标注比，射箭时长 (shot time，靠位到撒放) MAE 0.34 s（30 fps）。**这个数字就是我说"出箭时刻要交给 IMU"的依据。**
- [Park & Choi 2026](https://reference-global.com/article/10.2478/ijcss-2025-0016)：用 4 名韩国国家级选手的 17 关节序列训练序列网络。你只做自己跟自己比，这种跨人对比的东西用不上，但它的阶段切分可以参考。

## 开源方案对比

先把你说的一点纠正一下：OpenCap Monocular **并不是只能云端跑**，代码是开源的（`github.com/utahmobl/opencap-monocular`）。但它依赖 WHAM 加上一步优化，本地跑等于需要 GPU 做离线处理，实时是不可能的；而且它只在走路、深蹲、坐站这些动作上验证过。只看平面的话，它属于杀鸡用牛刀。

| 方案 | 相机数 | 本地 / 云 | 实时 | 输出 | 我的判断 |
| --- | --- | --- | --- | --- | --- |
| [Sports2D](https://www.github.com/davidpagnon/Sports2D) | 1 | 本地，pip 装 | 支持摄像头实时 | 2D 关节角 (joint angles)、节段角 (segment angles)；可按身高或相机距离做透视补偿 (perspective correction) | **第一阶段首选**。前提是动作在同一个平面里 (planar motion)（矢状面或冠状面都行） |
| [rtmlib](https://pypi.org/project/rtmlib)（RTMPose / RTMW） | 任意 | 本地，ONNX，不依赖 mmcv | RTMPose-m 在 i7 CPU 上 90+ FPS（[论文](https://ar5iv.labs.arxiv.org/html/2303.07399)） | 17/26 个身体点；RTMW 有 133 个全身点 (whole-body keypoints)，含手和脸 | 自己写 pipeline 时的底层。嘴角和拇指要用 RTMW |
| MediaPipe Pose | 1 | 本地 | CPU 实时 | 33 个点 | Lee 2024 就是用它做的。起步最快，精度一般 |
| [Pose2Sim](https://pypi.org/project/pose2sim/0.10.1) | ≥2 | 本地 | 离线 | 3D 关键点 + OpenSim 关节角 | **第二阶段**：加一台身后或头顶相机看 line 时用。作者与 Sports2D 是同一人，数据格式通用 |
| [FreeMoCap](https://github.com/madenmud/freemocap) | ≥2（建议 3+） | 本地 | 主要离线 | 3D 骨架，可导出 Blender | Pose2Sim 的备选。标定 (camera calibration) 用 ChArUco + Anipose |
| [OpenCap Monocular](https://mobl.mech.utah.edu/opencap-monocular-released/) | 1 台 iPhone | 默认走云，代码开源 | 否 | 3D 运动学 (kinematics) + 动力学 (kinetics)；转角误差 (rotational error) 4.8°（跟标记点 mocap 比） | 不建议走主线：上肢精细动作 (fine upper-limb motion) 没验证过，也做不了实时 |

## 给 Claude Code 的里程碑 (milestones)

**开发策略：先在笔记本上用 Python 做参考实现 (reference implementation)，再移植到 Android。** 理由是用录好的视频在笔记本上迭代，比每次改完都去靶场实测快得多，而且 Claude Code 能自己跑测试。具体做法：

- 阶段识别和指标写成与平台无关的纯函数，输入是关键点时间序列 (keypoint time series)，不碰视频和 UI。
- 固定一套"测试视频 + 期望输出"(golden fixtures)。以后 Android 移植的验收标准，就是在同一套测试视频上复现 Python 参考实现的输出。
- 姿态模型可替换。手机端的候选是 MediaPipe Pose Landmarker 或 RTMPose。RTMPose-s 论文报的是骁龙 865 上 70+ FPS（[来源](https://cdn05042023.gitlink.org.cn/OpenMMLab/mmpose/src/branch/main/projects/rtmpose)），你的手机实测多少以实测为准。

1. **M0 录数据（你做）**
   - 手机放侧面三脚架，60 fps 以上，录 30–50 箭。
   - 定一套相机摆放流程 (camera placement protocol)：地上贴标记，固定距离和高度；每次开录先拍一张标定物（已知长度的东西，或者 ChArUco 板）。开放式站姿下，这一步是箭与箭、日与日之间可比的前提。
2. **M1 离线骨架（Python，笔记本）**
   - 视频 → 关键点 → 阶段识别 → 指标 → 每箭记录 (JSON) → HTML 复盘报告。
   - 指标用注册制 (metric registry)，第一批重点放在收势 (follow-through)，靠位 (anchor) 指标做成可选。
   - 验收：你的 30 箭全部跑通；手标 5 箭的事件时刻，误差小于 2 帧。
3. **M2 模拟实时（还是 Python）**
   - 同一条流程改成逐帧的因果处理 (causal, streaming)，不允许看未来帧。在满弓阶段触发同步反馈规则，并记录每次触发的时刻和延迟。
   - 验收：流式结果和 M1 离线结果一致。延迟上限先不定，等 M2 测出真实数字再定。
4. **M3 Android 原型**
   - 相机预览 + 本地姿态估计 + 阶段识别 + 同步反馈。反馈先用手机自己的振动占位，每箭记录存在本地。
   - 验收：回放测试视频时输出与 Python 参考一致；实拍 30 箭不漏箭、不重复计箭。
5. **M4 箭后链路**
   - 手机上箭后几秒给终末反馈 (terminal feedback)，以收势为主。
   - 每箭记录上传到云 → 用更重的模型重分析 → 笔记本或网页上看复盘。云服务具体选哪家，到时再定。
6. **M5 接 M5Stick（等那边稳定了再做）**
   - 振动输出、撒放时间戳输入、两边时钟对齐 (time sync)。在这之前只保留接口。
7. **M6（可选）第二台相机**
   - 加身后或头顶的相机，看 line。

### 同步反馈可能有学习代价

拉弓中的同步反馈 (concurrent feedback) 有一个运动学习 (motor learning) 里很经典的问题：练习时表现变好，但反馈一撤，保持测试 (retention test) 里反而变差，也就是 guidance hypothesis 说的对反馈产生依赖（[Sigrist et al. 2013 综述](https://www.doi.org/10.3758/S13423-012-0333-8)；[2025 年的实验](https://www.frontiersin.org/journals/psychology/articles/10.3389/fpsyg.2025.1638827/full)）。降低同步反馈的频率可以减少这种依赖，一箭给同步加终末反馈、下一箭不给的交替方案效果更好（[Park et al. 2000](https://informahealthcare.com/doi/abs/10.1080/00222890009601379)）。设计上的结论：

- 反馈频率、逐步撤除的方式 (fading schedule) 要做成可配置项，不要写死成"每箭都震"。
- 每次练习要记下反馈是开是关。这样以后你能在自己身上测保持效果，而不只是看当天练得好不好。

## 还没定的事（故意留到后面）

这些不影响第一版骨架，但代码里要给它们留好位置。

- **数据标签和训练** ：先把架构搭出来，再谈数据从哪来。每箭记录里先留好结果 (outcome) 字段（环数、落点、自评分），可以先空着。
- **反馈通道** ：振动为主，耳机待定。代码里反馈是可插拔的输出接口。
- **交付形式** ：再议。M1 先出 HTML 报告，M3 是 Android app。
- **M5Stick 协议** ：跟 M5Stick 项目一起演化。
- **云服务选型** ：到 M4 再定。
- **给别人用时的个人配置 (profile)** ：身高、左右手、有没有靠位、站姿。

## 给 Claude Code 的工作约定

- **先能跑，再好看** ：每个里程碑先交一个端到端能跑通的粗糙版，再 refine。可以 refactor，但 refactor 前后都要过 golden fixtures 测试。
- **模块边界按架构草图来** ：视频输入、姿态估计、阶段识别、指标、反馈规则、反馈输出、存储、报告。换掉其中一块不该动到别的。
- **该我拍板的事不要自己定死** ：比如指标阈值、反馈规则、UI 取舍。记到 DECISIONS.md 里，列出选项等我定。
- **不碰 M5Stick 项目的代码** ：只按接口写桩 (stub)。
- **每个里程碑收尾** ：更新 README，附一个能直接跑的 demo 命令。

## 参考链接

- [Sports2D (GitHub)](https://www.github.com/davidpagnon/Sports2D) · [JOSS 论文](https://www.theoj.org/joss-papers/joss.06849/10.21105.joss.06849.pdf)
- [Pose2Sim (PyPI)](https://pypi.org/project/pose2sim/0.10.1)
- [rtmlib](https://pypi.org/project/rtmlib) · [RTMPose 论文](https://ar5iv.labs.arxiv.org/html/2303.07399) · [RTMW 论文](https://arxiv.org/abs/2407.08634)
- [FreeMoCap](https://github.com/madenmud/freemocap)
- [OpenCap Monocular 发布说明](https://mobl.mech.utah.edu/opencap-monocular-released/) · [arXiv 2603.24733](https://arxiv.org/abs/2603.24733) · [CVBW 2026 版](https://cvbw2026.github.io/assets/papers/15.pdf)
- [BowPose / ShotLab](https://huggingface.co/jotux/BowPose)
- [Mantis X8 介绍](https://www.americanhunter.org/content/first-look-mantis-x8-archery-trainer) · [Bow International 测评](https://www.bow-international.com/?p=4652)
- [Steady-Aim A1](https://lancasterarchery.com/products/steady-aim-a1-shooting-analysis-system)
- [Garmin ArcheryX](https://the5krunner.com/2026/08/25/garmin-archeryx-explained/)
- [Lee et al. 2024, Applied Sciences: 靠位到撒放的时长分析](https://mdpi.com/2076-3417/14/24/11838)
- [Park & Choi 2026, IJCSS: 射箭动作序列网络](https://reference-global.com/article/10.2478/ijcss-2025-0016)
