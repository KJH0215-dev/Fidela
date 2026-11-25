# Fidela

OBJ 파일을 읽어 3D 메시를 시각화하고 분석하는 C++ 프로젝트입니다.

## 스크린샷
![실행 화면 1](<스크린샷 2026-11-23 232801.png>)
![실행 화면 2](<스크린샷 2026-11-23 235954.png>)

## 주요 기능
- OBJ 파서: 정점(v), 법선(vn), 면(f) 등을 읽어 메시로 변환
- 메시 진단: 메시 데이터의 상태를 검사
- 수식 기반 곡면(Surface) 생성
- 카메라 조작 및 실시간 렌더링
- ImGui 기반 UI

## 개발 환경
- Windows, Visual Studio 2026
- C++17 이상
- 렌더링: OpenGL (GLEW, GLFW)

## 의존성 (vcpkg)
이 저장소에는 외부 라이브러리가 포함되어 있지 않습니다. vcpkg로 직접 설치해 주세요.

```
vcpkg install imgui[glfw-binding,opengl3-binding] glfw3 glew
vcpkg integrate install
```

## 빌드 및 실행
1. `Vector3_visualize.slnx`를 Visual Studio로 엽니다.
2. x64, Debug 또는 Release로 빌드합니다.
3. 실행 후 `test_sphere_broken.obj`(의도적으로 깨뜨린 테스트 모델)를 불러와 메시 진단 기능을 확인합니다.

## 구조
- `Objparser` : OBJ 파일 파싱
- `Mesh`, `MeshDiagnostics` : 메시 데이터와 진단
- `Surface`, `Expression` : 곡면 생성과 수식 처리
- `camera` : 카메라
- `Vector3` : 벡터 연산
- `vector_visual.cpp` : 메인 및 UI
