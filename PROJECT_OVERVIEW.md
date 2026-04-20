# gs_mfc_ui (Toruss2) 프로젝트 개요

## 1) 한 줄 요약
`gs_mfc_ui`는 **Windows MFC(C++) 기반 통합 관제 UI 애플리케이션(Toruss2)** 으로,
컬러/열화상 RTSP 영상, 메인뷰/미니맵/파노라마 표시, PTZ 제어, 객체 추적(MOTWrapper), MongoDB 기반 장비 프로파일 조회 기능을 제공합니다.

---

## 2) 저장소 구조
- `Toruss2.sln`
  - Visual Studio 솔루션 파일
- `Toruss2/`
  - 실제 애플리케이션 소스(C++/MFC)
- `TorussLog/`
  - 런타임 로그 파일 저장 폴더 (예: `Log_20260306.txt`)

---

## 3) 기술 스택 및 외부 의존성
- **언어/프레임워크**: C++, MFC (`UseOfMfc=Dynamic`)
- **플랫폼/빌드 구성**: Windows, `Debug/Release` × `Win32/x64`
- **영상 처리/디코딩**:
  - OpenCV (`opencv_world4120[d].lib`)
  - FFmpeg 계열 헤더 사용 (`libavformat`, `libavcodec`, `libavutil`, `libswscale`)
- **AI/추적**:
  - `MOTWrapper.lib`
  - ONNX 모델 파일 로드: `\Models\best.onnx`
- **DB 연동**:
  - MongoDB C++ Driver (`mongocxx`, `bsoncxx`)
  - 기본 연결 URI: `mongodb://127.0.0.1:27017`
  - 컬렉션: `toruss.device_profile`
- **그래픽**: GDI+ (`gdiplus.lib`)

> 참고: `.vcxproj`에 절대경로(`E:\...`, `C:\opencv\...`)가 포함되어 있어,
> 다른 개발환경에서는 Include/Lib 경로를 반드시 재설정해야 합니다.

---

## 4) 앱 실행 흐름 (핵심)
1. `CToruss2App::InitInstance()`에서 GDI+ 초기화 및 DPI 인식 설정.
2. 메인 다이얼로그 `CToruss2Dlg` 생성 및 `DoModal()` 실행.
3. 다이얼로그 초기화 단계에서:
   - 로고 로딩(`\img\Logo.png`)
   - AI 모델 로딩(`\Models\best.onnx`)
   - MongoDB 연결 시도
   - 페이지/컨트롤/핫키/타이머 구성
4. 장비 선택 후 RTSP URL 생성 및 디코더/렌더 파이프라인 활성화.
5. 사용자 입력(키/마우스), 타이머, 메시지 루프를 통해 PTZ 명령/화면 갱신 수행.
6. 앱 종료 시 `ExitInstance()`에서 GDI+ 종료.

---

## 5) 주요 모듈 역할 (파일 기준)
- `Toruss2/Toruss2.cpp`, `Toruss2/Toruss2.h`
  - 앱 엔트리 포인트, 앱 전역 초기화/종료 수명주기.
- `Toruss2/Toruss2Dlg.cpp`, `Toruss2/Toruss2Dlg.h`
  - 메인 UI, 페이지 전환(Connect/Gear/Video), 초기화/핫키/메시지 처리, 전체 오케스트레이션.
- `Toruss2/VideoView.cpp`, `Toruss2/VideoView.h`
  - 멀티 비디오 뷰 관리, 프레임 합성/표시, 추적 연동 PTZ 동작.
- `Toruss2/CMainView.cpp`, `Toruss2/CMainView.h`
  - 메인 뷰 마우스 상호작용, 방향 오버레이, PTZ 시작/정지 입력 처리.
- `Toruss2/CRtspDecoder.cpp`, `Toruss2/CRtspDecoder.h`
  - FFmpeg 기반 RTSP 디코딩 처리.
- `Toruss2/RtspBuild.cpp`, `Toruss2/RtspBuild.h`
  - 장비/설정 기반 RTSP URL 구성.
- `Toruss2/CControl_*.cpp/.h`
  - 컬러/열화상/파노라마/PTZ 제어 로직 캡슐화.
- `Toruss2/MongoDB.cpp`, `Toruss2/MongoDB.h`
  - MongoDB 연결, 장비 목록/프로파일 조회.
- `Toruss2/Logger.cpp`, `Toruss2/Logger.h`
  - 로그 파일 생성/기록 유틸리티.

---

## 6) 주요 기능 요약
- **멀티 영상 UI**: 컬러/열화상/미니맵/메인뷰 구성 및 레이아웃 전환
- **RTSP 스트리밍/디코딩**: 네트워크 스트림 수신 후 프레임 렌더링
- **PTZ 제어**:
  - 수동 PTZ 속도(0~63) 조절
  - 마우스 방향 기반 제어 및 정지
  - 트래킹/뷰포인트 기준 보정 이동
- **AI 추적 연동**: MOTWrapper 기반 탐지/추적 결과를 뷰 및 PTZ 로직에 반영
- **MongoDB 연동**: 장비 목록 및 상세 프로파일 조회
- **시스템 정보/보조 처리**: CPU/RAM 사용률 계산 유틸, 로그 기록

---

## 7) 빌드/실행 전 체크리스트
- [ ] Visual Studio 2022 + MFC/C++ 개발환경 설치
- [ ] `Toruss2.sln` 로드 후 `x64` 구성 우선 확인 (`Debug|x64` 권장)
- [ ] OpenCV / FFmpeg / MongoDB C++ Driver 경로 설정
- [ ] `MOTWrapper.lib` 링크 가능 상태 확인
- [ ] 실행 경로 기준 리소스 준비
  - [ ] `\Models\best.onnx`
  - [ ] `\img\Logo.png`
- [ ] MongoDB 실행 및 `toruss.device_profile` 데이터 확인

---

## 8) 현재 코드 기준 주의사항
- 프로젝트 파일에 개발 PC 절대경로가 포함되어 있어 이식성이 낮습니다.
- 외부 라이브러리/모델/리소스가 누락되면 빌드 또는 실행 시 실패할 수 있습니다.
- 메인 창 크기/레이아웃 관련 상수(예: 1920x1080)가 코드에 존재하므로,
  디스플레이 환경 차이에 따른 UI 확인이 필요합니다.

---

## 9) 개선 제안 (문서/구성)
1. 절대경로 의존성을 `.props`/환경변수 기반으로 표준화
2. 루트 `README.md`에 설치/빌드/실행/트러블슈팅 통합 문서화
3. 실행 전 의존성 점검(모델/리소스/DB/DLL) 스크립트 추가
4. 배포 환경을 고려해 로그/설정 경로 정책(AppData 등) 정리

