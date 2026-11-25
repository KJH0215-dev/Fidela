# Fidela

OBJ 파일을 읽어 3D 메시를 시각화하고 분석하는 C++ 프로젝트입니다.

## 주요 기능
- OBJ 파서: 정점(v), 법선(vn), 면(f) 등을 읽어 메시로 변환
- 메시 진단: 메시 데이터의 상태를 검사
- 수식 기반 곡면(Surface) 생성
- 카메라 조작 및 실시간 렌더링
- ImGui 기반 UI

## 개발 환경
- Windows, Visual Studio 2026
- C++17 이상
- 렌더링: OpenGL

## 의존성 (vcpkg)
이 저장소에는 외부 라이브러리가 포함되어 있지 않습니다. vcpkg로 직접 설치해 주세요.

    vcpkg install imgui[glfw-binding,opengl3-binding] glfw3
    vcpkg integrate install

## 빌드 및 실행
1. `Vector3_visualize.slnx`를 Visual Studio로 엽니다.
2. x64, Debug 또는 Release로 빌드합니다.
3. 실행 후 OBJ 파일을 불러옵니다. 샘플 모델은 포함되어 있지 않으니 직접 준비해 주세요.

## 구조
- `Objparser` : OBJ 파일 파싱
- `Mesh`, `MeshDiagnostics` : 메시 데이터와 진단
- `Surface`, `Expression` : 곡면 생성과 수식 처리
- `camera` : 카메라
- `Vector3` : 벡터 연산
- `vector_visual.cpp` : 메인 및 UI
