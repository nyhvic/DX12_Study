# 6.1~6.6  

## 6.1 Vertices, Input layouts  
vertex 구조체 - pos, normal, color, texture 등등 여러 정보 가짐  
D3D12_INPUT_LAYOUT_DESC 구조체 - ID3D12_INPUT_ELEMENT_DESC 배열. 정점 descriptor    
```c++
typedef struct D3D12_INPUT_ELEMENT_DESC
{
    LPCSTR SemanticName; //변수이름, 정점 셰이더 입력과 매핑
    UINT SemanticIndex; //이름 구분 인덱스
    DXGI_FORMAT Format; //유형, ex)DXGI_FORMAT_R32G32B32_FLOAT
    UINT InputSlot; //데이터 어느 버퍼에서 오는지
    UINT AlignedByteOffset; //정점 구조체 멤버 바이트 위치
    D3D12_INPUT_CLASSIFICATION InputSlotClass;
    UINT InstanceDataStepRate; // 나중에 쓴단다.. 인스턴싱과 관련
} D3D12_INPUT_ELEMENT_DESC;


//ex) vertex : {XMFLOAT3 : pos, XMFLOAT4 : color}

D3D12_INPUT_ELEMENT_DESC desc1[] =
{
  {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 0,  D3D12_INPUT_PER_VERTEX_DATA, 0},
  {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,0, 12, D3D12_INPUT_PER_VERTEX_DATA, 0}
};
```

## 6.2 Vertex buffers  
GPU 정점 배열 접근 - GPU resource(buffer, ID3D12Resource) 필요  
Vertex buffer - 정점 저장하는 버퍼  
D3D12_RESOURCE_DESC 채우고, ID3D12Device::CreateCommittedResource로 생성  
```cpp
//D3dx12.h CD3DX12_RESOURCE_DESC 클래스

//버퍼 descriptor 생성 간소화
static inline CD3DX12_RESOURCE_DESC Buffer(
 UINT64 width, //버퍼 저장된 바이트 수
 D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
 UINT64 alignment = 0 )
{
 return CD3DX12_RESOURCE_DESC( D3D12_RESOURCE_DIMENSION_BUFFER,
 alignment, width, 1, 1, 1,
 DXGI_FORMAT_UNKNOWN, 1, 0,
 D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags );
}

//텍스쳐 리소스 descriptor 구성 메소드
CD3DX12_RESOURCE_DESC::Tex1D
CD3DX12_RESOURCE_DESC::Tex2D
CD3DX12_RESOURCE_DESC::Tex3D
```
D3D12_HEAP_TYPE_DEFAULT - static geometry 정점 버퍼 저장(GPU 효율), CPU가 쓰기 어려움  
static geometry - 움직임 없는 구조(나무, 지형등등)  
D3D12_HEAP_TYPE_UPLOAD에 중간 버퍼, CPU가 upload heap에 정점 복사 후 default로 옮김(GPU가 복사 명령 처리 이전까지는 upload heap 유지)  
책에선 d3dUtil::CreateDefaultBuffer함수 만들어 한번에 관리  

Vertex Buffer View  
vertex buffer pipeline에 bind 하기 위해 사용  
```cpp
//vertex buffer view 표현하는 구조체
typedef struct D3D12_VERTEX_BUFFER_VIEW
{
 D3D12_GPU_VIRTUAL_ADDRESS BufferLocation; //GPU 가상 주소(버퍼 위치)
 UINT SizeInBytes; //버퍼 크기
 UINT StrideInBytes; //정점 하나 크기
} D3D12_VERTEX_BUFFER_VIEW;

//버퍼 파이프라인 입력 슬롯에 바인딩
void ID3D12GraphicsCommandList::IASetVertexBuffers(
 UINT StartSlot, // 버퍼 바인딩 시작할 0~15까지 16개 입력 슬롯
 UINT NumBuffers, //버퍼 개수
 const D3D12_VERTEX_BUFFER_VIEW *pViews); //vertex buffer view 첫번째 요소

//실제 그릴 때 사용
 void ID3D12CommandList::DrawInstanced(
 UINT VertexCountPerInstance, //그릴 정점 수
 UINT InstanceCount, //인스턴싱에 사용, 인스턴스 수
 UINT StartVertexLocation, //정점 버퍼 첫번째 정점 인덱스
 UINT StartInstanceLocation); //인스턴싱

//primitive topology 설정 필요
cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
```

## 6.3 Index, Index buffer  
index buffer - 정점 번호 배열 저장  
책에서 d3dUtil::CreateDefaultBuffer로 생성  
index buffer view로 파이프라인에 바인딩
```cpp
typedef struct D3D12_INDEX_BUFFER_VIEW
{
 D3D12_GPU_VIRTUAL_ADDRESS BufferLocation; //GPU 가상 주소
 UINT SizeInBytes; //인덱스 버퍼 크기
 DXGI_FORMAT Format; 
 //인덱스 크기, DXGI_FORMAT_R16_UINT or DXGI_FORMAT_R32_UINT
} D3D12_INDEX_BUFFER_VIEW;

//IA에 바인딩
mCommandList->IASetIndexBuffer(&ibv);
```

```cpp
//인덱스 그리기
void ID3D12GraphicsCommandList::DrawIndexedInstanced(
    UINT IndexCountPerInstance,     // 인덱스 개수 (삼각형 1개당 3)
    UINT InstanceCount,             // 인스턴스 수 (보통 1)
    UINT StartIndexLocation,        // 인덱스 버퍼에서 시작 인덱스
    INT BaseVertexLocation,         // 정점 인덱스에 더할 오프셋
    UINT StartInstanceLocation      // 인스턴스 오프셋 (보통 0)
);
```
vertex buffer, index buffer - 하나로 합쳐서 사용, BaseVertexLocation으로 계산

## 6.4 Vertex Shader
shader - HLSL로 작성
vertex shader - vertex clip space 변환, 색상 전달 등
```cpp
//교재 hlsl 예제
cbuffer cbPerObject : register(b0) //const buffer, register b0에 바인딩
{
    float4x4 gWorldViewProj;  //행렬
};

struct VertexIn //정점 위치, 색상 input
{
    float3 PosL : POSITION;
    float4 Color : COLOR; 
};

struct VertexOut
{
    float4 PosH : SV_POSITION; //clip space 위치
    float4 Color : COLOR; //pixel shader에 전달할 색상
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
    vout.Color = vin.Color;
    return vout;
}

//dx12 5.1부터, shader model 사용
// 1. 먼저 구조체 정의
struct ObjectConstants {
    float4x4 gWorldViewProj;
    uint matIndex;
};

// 2. 구조체로 상수 버퍼 정의
ConstantBuffer<ObjectConstants> gObjConstants : register(b0);

// 3. 멤버 접근
uint index = gObjConstants.matIndex;
float4x4 mat = gObjConstants.gWorldViewProj;
//명시적으로 네임스페이스를 쓸 수 있고, 타입 안전성, 다중 상수 버퍼 배열 지원 등등을 할 수 있다고 한다...
```
geometry shader 없는 경우 SV_POSITION(clip space) 좌표 필수
vertex 시멘틱과 연결 필요, out oPosH - SV_POSITION으로 설정해야 함  

## 6.5 Pixel Shader  
rasterization 이후 fragment마다 실행, 색상 값 계산
```cpp
//교재 예제
struct VertexOut  // 정점 셰이더 출력 = 픽셀 셰이더 입력
{
    float4 PosH : SV_POSITION;
    float4 Color : COLOR;
};

float4 PS(VertexOut pin) : SV_Target  // pin = Pixel INput
{
    return pin.Color; //rasterization에서 보간된 색 그대로 반환
}
```
SV_TARGET -> Redder Target  
ealry-z - pixel shader 이전에 깊이 테스트, pixel shader에서 z값 수정하면 하지않음  

## 6.6 Constant Buffers  
constant buffer - shader program에서 참조될 수 있는 GPU resource  
일반적으로 프레임마다 한번씩 업데이트 -> upload heap에 생성  
크기 - 256바이트 배수여야 함  
```cpp
//같은 타입 여러 상수 버퍼 ex) 객체마다 다른 WorldViewProj mat, 생성 예제
struct ObjectConstants
{
 DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};
UINT elementByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants)); //256 배수로 설정, 이런 함수 만들어 쓰면 좋을듯
ComPtr<ID3D12Resource> mUploadCBuffer;
device->CreateCommittedResource(
 &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
 D3D12_HEAP_FLAG_NONE,
 &CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * NumElements),
 D3D12_RESOURCE_STATE_GENERIC_READ,
 nullptr,
 IID_PPV_ARGS(&mUploadCBuffer));
 ```
 constant buffer 데이터 업데이트  
 ```cpp
 //constant buffer 리소스 데이터 포인터 얻기(Map)
ComPtr<ID3D12Resource> mUploadBuffer;
BYTE* mMappedData = nullptr;
mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData));

//시스템 메모리에서 const buffer로 데이터 복사
memcpy(mMappedData, &data, dataSizeInBytes);

//사용 이후 매핑 해제(Unmap)
if(mUploadBuffer != nullptr)
mUploadBuffer->Unmap(0, nullptr);
mMappedData = nullptr;

//ai가 뽑아준 사용 예제
void UpdateConstantBuffer(int objectIndex)
{
    ObjectConstants objCB;
    XMStoreFloat4x4(&objCB.WorldViewProj, XMMatrixMultiply(world, viewProj));
    
    BYTE* mappedData = nullptr;
    
    // 1. Map
    ThrowIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));
    
    // 2. 올바른 오프셋 계산 (중요!)
    BYTE* destination = mappedData + (objectIndex * mElementByteSize);
    
    // 3. memcpy (실제 데이터 크기만!)
    memcpy(destination, &objCB, sizeof(objCB));
    
    // 4. Unmap
    mUploadBuffer->Unmap(0, nullptr);
}

void UpdateAllObjects()
{
    BYTE* mappedData = nullptr;
    mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    
    for(int i = 0; i < numObjects; i++)
    {
        ObjectConstants objCB = CalculateConstants(i);
        BYTE* dest = mappedData + (i * mElementByteSize);
        memcpy(dest, &objCB, sizeof(objCB));
    }
    
    mUploadBuffer->Unmap(0, nullptr);  // 한 번만!
}
 ```
 위 내용들 활용해서 upload buffer helper class 만들어 사용하면 좋다.  

 constant buffer descriptor  
 D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV(CBV, SRV, UAV 저장 가능) 타입 디스크립터 힙 생성,  
 ID3D12Device::CreateConstantBufferView로 constant buffer view 생성  
```cpp
//descirptor heap 생성 예제
D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
cbvHeapDesc.NumDescriptors = 1;
cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; //셰이더에서 접근 간으
cbvHeapDesc.NodeMask = 0;
ComPtr<ID3D12DescriptorHeap> mCbvHeap
md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap));

//constant buffer view 생성 예제
//객체마다 가질 데이터 
struct ObjectConstants
{
 XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

//n개 객체 const 저장하는 constant buffer
std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;
mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), n, true);

//i번째 객체 CBV 생성
UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
//전체 버퍼 시작 주소
D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
//i번째 객체 constant buffer
int boxCBufIndex = i;
cbAddress += boxCBufIndex*objCBByteSize;
//cvAddress(i번째 객체 constant buffer view)에 CBV 생성
D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
cbvDesc.BufferLocation = cbAddress;
cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
md3dDevice->CreateConstantBufferView(&cbvDesc,mCbvHeap->GetCPUDescriptorHandleForHeapStart());
```
실습좀 하면서 이해해야할듯, 아직은 그렇다~ 정도만 이해하는 느낌  

셰이더 프로그램 - resource 레지스터 슬롯에 바인딩, 셰이더에서 접근해 사용, 셰이더마다 접근할 리소스, 레지스터 다름  
root signature - draw call 이전에 바인딩할 리소스와 매핑되는 레지스터 정의
shader - 함수, root signature - 함수 매개변수 정의  
ID3D12RootSignature  
root signature - root parameter(root constant, root descriptor, descriptor table)
descriptor table - descirptor heap 연속된 descriptor 범위 가리키는 포인터

```cpp
//아직은 이 부분만 이해하면 된다
CD3DX12_ROOT_PARAMETER slotRootParameter[1]; //root parameter 한개만 사용

//descriptor table - CBV 하나 구간 가르킴
CD3DX12_DESCRIPTOR_RANGE cbvTable;
cbvTable.Init(
 D3D12_DESCRIPTOR_RANGE_TYPE_CBV, //CBV table
 1, //CBV descirptor 하나
 0); //register 지정

slotRootParameter[0].InitAsDescriptorTable(
 1, // Number of ranges
 &cbvTable); // Pointer to array of ranges

//root signature 정의
 CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
    1, //root parameter 개수
    slotRootParameter, 
    0, 
    nullptr, 
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

//root signature 생성
ComPtr<ID3DBlob> serializedRootSig = nullptr;
ComPtr<ID3DBlob> errorBlob = nullptr;
HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc,
 D3D_ROOT_SIGNATURE_VERSION_1,
 serializedRootSig.GetAddressOf(),
 errorBlob.GetAddressOf());
 ThrowIfFailed(md3dDevice->CreateRootSignature(
 0,
 serializedRootSig->GetBufferPointer(),
 serializedRootSig->GetBufferSize(),
 IID_PPV_ARGS(&mRootSignature)));
 ```