Input Assembler (IA) Stage 

Vertex Shader (VS) Stage 

Hull Shader (HS) Stage 

Tessellator Stage 

Domain Shader (DS) Stage 

Geometry Shader (GS) Stage 

Stream Output (SO) Stage 

Rasterizer (RS) Stage 

Pixel Shader (PS) Stage 

Output Merger (OM) Stage

## IA  
메모리에서 정점,인덱스 읽어와 조립  

정점들 vertex buffer에 저장, typedef로 지정된 enum primitive topology이용해 그리는 방식 설정(IASetPrimitiveTopology)  

point list, line strip, line list, triangle strip, triangle list, primitives with adjacency, control pointpatch lsit  

테셀레이션 단계에서는 patch를 기본 단위로 쓴다고 한다…  

인덱스배열로 메모리 효율, 정점 캐시 활용  

## VS  
input : vertex, output : vertex

모든 정점에 대해 진행, 변환, 조명, 변위 매핑

오브젝트들 local coordinate 기준으로 구성(쉬움, 재사용성, 효율)

world transform - local coordinate → world coordinate

world matrix - S R T 곱해서 row - matrix (matrix col은 TRS) or 로컬좌표계 원점,x,y,z축

view space - 카메라 원점, 바라보는 방향 +z, y up

view matrix - 카메라 matrix(RT) 역행렬 = $T^{-1}R^T$

카메라 위치 Q, object T일때 z축  $w = T-Q/ ||T-Q||$, x축  $u = j\times w / || j\times w||$ y축 wxu , directx XMMatrixLookAtLH

homogeneous clip space - frustum 공간

근평면 n, 원평면 f, (수직)FOV α, 종횡비 r, r=w/h

원평면 높이 2 가정, 거리 $d = 1/tan(\alpha/2)$

수평 시야각은 거리 d와 종횡비 r 이용해 구함 $tan(\beta/2) = r/d$

view 공간 좌표(x,y,z) 투영시 xd/z, yd/z

NDC - 투영된 좌표 [-1,1]로 정규화, 하드웨어 편리성 x`=xd/rz, y`=yd/z

perspective matrix - x,y → x`, y` 되도록 설정, 통일된 행렬 위해 z로 나누는 작업 따로 진행, z값 정규화 위해 n,f사용 

XMMatrixPerspectiveFOVLH  

## 테셀레이션  
mesh 삼각형 새로운 더 작은 삼각형으로 나눔

LOD 구현 시 가까운 object mesh 더 많은 삼각형으로 표현

메모리에 그리는것보다 적은 삼각형이 들어가 메모리 절약

high poly mesh는 렌더링에만 사용하고 low poly mesh는 애니매이션 등에만 사용, 효율

선택적으로 사용, dx11이후 GPU에서 바로 실행 가능  

## GS  
이것도 선택

geometry 도형 입력받아 geometry 생성, 삭제

stream out - 만든 geometry 렌더링에 쓰도록 메모리에 쓸 수 있다는듯  

## Clipping  
frustum 밖에 있는 부분 자르기(top bottom left right near far 평면)

ncd기준 x,y좌표 [-1,1], z좌표 [0,1] → clip space에서 [-w,w], [0,w]범위인지 확인  
local - world - camera - clip - ndc 생각  

## Rasterizer  
viewport transform - 픽셀 좌표로 변환, z값은 depth buffer에서 사용

backface culling - 삼각형 face normal과 카메라 보는방향 비교(내적)해서 뒷면 삼각형 렌더링에서 제거, directx - 보는사람 기준 시계방향 감긴 삼각형 앞면

vertex attribute interpolation - 삼각형 내부 점 정점 이용해 보간, 정점에 할당된 색, 법선, 텍스처, 깊이 값 등등 연결

perspective correct interpolation 사용해 보간 (수업때 나온듯, 투영되기 전 좌표로 보간) 알 필요 없다네요  

## PS  
각 픽셀마다 실행, 색상, 조명, 반사, 그림자 등등  

## OS
PS에서 나온 pixel fragment

depth, stencil test → back buffer → blending