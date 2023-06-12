#include "rooms/roomDemoShadow.h"
#include <array>
#include "mesh.h"

using namespace mini;
using namespace gk2;
using namespace DirectX;
using namespace std;

const XMFLOAT3 RoomDemoShadow::TEAPOT_POS{ -1.3f, -0.74f, -0.6f };
const XMFLOAT4 RoomDemoShadow::TABLE_POS{ 0.5f, -0.96f, 0.5f, 1.0f };

RoomDemoShadow::RoomDemoShadow (HINSTANCE appInstance)
	: DxApplication (appInstance, 1280, 720, L"Pokój"),
	//Constant Buffers
	m_cbWorldMtx (m_device.CreateConstantBuffer<XMFLOAT4X4> ()),
	m_cbProjMtx (m_device.CreateConstantBuffer<XMFLOAT4X4> ()),
	m_cbViewMtx (m_device.CreateConstantBuffer<XMFLOAT4X4, 2> ()),
	m_cbSurfaceColor (m_device.CreateConstantBuffer<XMFLOAT4> ()),
	m_cbLightPos (m_device.CreateConstantBuffer<XMFLOAT4> ()),
	m_cbMapMtx (m_device.CreateConstantBuffer<XMFLOAT4X4> ()),
	//Textures
	m_smokeTexture (m_device.CreateShaderResourceView (L"resources/textures/smoke.png")),
	m_opacityTexture (m_device.CreateShaderResourceView (L"resources/textures/smokecolors.png")),
	m_lightMap (m_device.CreateShaderResourceView (L"resources/textures/light_cookie.png")),
	//Particles
	m_particles{ {-1.3f, -0.6f, -0.14f} } {
	//Projection matrix
	auto s = m_window.getClientSize ();
	auto ar = static_cast<float>(s.cx) / s.cy;
	XMStoreFloat4x4 (&m_projMtx, XMMatrixPerspectiveFovLH (XM_PIDIV4, ar, 0.01f, 100.0f));
	UpdateBuffer (m_cbProjMtx, m_projMtx);
	UpdateCameraCB ();

	// TODO : 1.02 Calculate light projection matrix;
	XMStoreFloat4x4 (&m_lightProjMtx, XMMatrixPerspectiveFovLH (LIGHT_FOV_ANGLE, 1.0f, LIGHT_NEAR, LIGHT_FAR));

	//Sampler States
	SamplerDescription sd;

	// TODO : 1.05 Create sampler with appropriate border color and addressing (border) and filtering (bilinear) modes
	sd.BorderColor[0] = 0.0f;
	sd.BorderColor[1] = 0.0f;
	sd.BorderColor[2] = 0.0f;
	sd.BorderColor[3] = 0.0f;

	sd.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sd.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;

	m_sampler = m_device.CreateSamplerState (sd);

	//Textures
	// TODO : 1.10 Create shadow texture with appropriate width, height, format, mip levels and bind flags
	Texture2DDescription td;
	td.Width = TEXTURE_SIZE;
	td.Height = TEXTURE_SIZE;
	td.Format = DXGI_FORMAT_R32_TYPELESS;
	td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	td.MipLevels = 1;

	auto shadowTexture = m_device.CreateTexture (td);

	DepthStencilViewDescription dvd;
	dvd.Format = DXGI_FORMAT_D32_FLOAT;

	// TODO : 1.11 Create depth-stencil-view for the shadow texture with appropriate format

	m_shadowDepthBuffer = m_device.CreateDepthStencilView (shadowTexture, dvd);

	ShaderResourceViewDescription srvd;
	srvd.Format = DXGI_FORMAT_R32_FLOAT;
	srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvd.Texture2D.MipLevels = 1;
	srvd.Texture2D.MostDetailedMip = 0;

	// TODO : 1.12 Create shader resource view for the shadow texture with appropriate format, view dimensions, mip levels and most detailed mip level

	m_shadowMap = m_device.CreateShaderResourceView (shadowTexture, srvd);

	//Meshes
	vector<VertexPositionNormal> vertices;
	vector<unsigned short> indices;
	m_wall = Mesh::Rectangle (m_device, 4.0f);
	m_teapot = Mesh::LoadMesh (m_device, L"resources/meshes/teapot.mesh");
	m_sphere = Mesh::Sphere (m_device, 8, 16, 0.3f);
	m_box = Mesh::ShadedBox (m_device);
	m_lamp = Mesh::LoadMesh (m_device, L"resources/meshes/lamp.mesh");
	m_chairSeat = Mesh::LoadMesh (m_device, L"resources/meshes/chair_seat.mesh");
	m_chairBack = Mesh::LoadMesh (m_device, L"resources/meshes/chair_back.mesh");
	m_monitor = Mesh::LoadMesh (m_device, L"resources/meshes/monitor.mesh");
	m_screen = Mesh::LoadMesh (m_device, L"resources/meshes/screen.mesh");
	m_tableLeg = Mesh::Cylinder (m_device, 4, 9, TABLE_H - TABLE_TOP_H, 0.1f);
	m_tableSide = Mesh::Cylinder (m_device, 1, 16, TABLE_TOP_H, TABLE_R);
	m_tableTop = Mesh::Disk (m_device, 16, TABLE_R);

	m_vbParticles = m_device.CreateVertexBuffer<ParticleVertex> (ParticleSystem::MAX_PARTICLES);

	//World matrix of all objects
	auto temp = XMMatrixTranslation (0.0f, 0.0f, 2.0f);
	auto a = 0.f;
	for (auto i = 0U; i < 4U; ++i, a += XM_PIDIV2)
		XMStoreFloat4x4 (&m_wallsMtx[i], temp * XMMatrixRotationY (a));
	XMStoreFloat4x4 (&m_wallsMtx[4], temp * XMMatrixRotationX (XM_PIDIV2));
	XMStoreFloat4x4 (&m_wallsMtx[5], temp * XMMatrixRotationX (-XM_PIDIV2));
	XMStoreFloat4x4 (&m_teapotMtx, XMMatrixTranslation (0.0f, -2.3f, 0.f) * XMMatrixScaling (0.1f, 0.1f, 0.1f) *
		XMMatrixRotationY (-XM_PIDIV2) * XMMatrixTranslation (-1.3f, -0.74f, -0.6f));

	XMStoreFloat4x4 (&m_sphereMtx, XMMatrixRotationY (-XM_PIDIV2) * XMMatrixTranslation (TEAPOT_POS.x, TEAPOT_POS.y, TEAPOT_POS.z));
	XMStoreFloat4x4 (&m_boxMtx, XMMatrixTranslation (-1.4f, -1.46f, -0.6f));
	XMStoreFloat4x4 (&m_chairMtx, XMMatrixRotationY (XM_PI + XM_PI / 9) *
		XMMatrixTranslation (-0.1f, -1.06f, -1.3f));
	XMStoreFloat4x4 (&m_monitorMtx, XMMatrixRotationY (XM_PIDIV4) *
		XMMatrixTranslation (TABLE_POS.x, TABLE_POS.y + 0.42f, TABLE_POS.z));
	a = XM_PIDIV4;
	for (auto i = 0U; i < 4U; ++i, a += XM_PIDIV2)
		XMStoreFloat4x4 (&m_tableLegsMtx[i], XMMatrixTranslation (0.0f, 0.0f, TABLE_R - 0.35f) * XMMatrixRotationY (a) *
			XMMatrixTranslation (TABLE_POS.x, TABLE_POS.y - (TABLE_H + TABLE_TOP_H) / 2, TABLE_POS.z));
	XMStoreFloat4x4 (&m_tableSideMtx, XMMatrixRotationY (XM_PIDIV4 / 4) *
		XMMatrixTranslation (TABLE_POS.x, TABLE_POS.y - TABLE_TOP_H / 2, TABLE_POS.z));
	XMStoreFloat4x4 (&m_tableTopMtx, XMMatrixRotationY (XM_PIDIV4 / 4) *
		XMMatrixTranslation (TABLE_POS.x, TABLE_POS.y, TABLE_POS.z));

	//Constant buffers content
	UpdateBuffer (m_cbSurfaceColor, XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f });

	//Render states
	RasterizerDescription rsDesc;
	rsDesc.CullMode = D3D11_CULL_NONE;
	m_rsCullNone = m_device.CreateRasterizerState (rsDesc);

	m_bsAlpha = m_device.CreateBlendState (BlendDescription::AlphaBlendDescription ());
	DepthStencilDescription dssDesc;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	m_dssNoWrite = m_device.CreateDepthStencilState (dssDesc);

	auto vsCode = m_device.LoadByteCode (L"phongVS.cso");
	auto psCode = m_device.LoadByteCode (L"phongPS.cso");
	m_phongVS = m_device.CreateVertexShader (vsCode);
	m_phongPS = m_device.CreatePixelShader (psCode);
	m_inputlayout = m_device.CreateInputLayout (VertexPositionNormal::Layout, vsCode);

	psCode = m_device.LoadByteCode (L"lightAndShadowPS.cso");
	m_lightShadowPS = m_device.CreatePixelShader (psCode);

	vsCode = m_device.LoadByteCode (L"particleVS.cso");
	psCode = m_device.LoadByteCode (L"particlePS.cso");
	auto gsCode = m_device.LoadByteCode (L"particleGS.cso");
	m_particleVS = m_device.CreateVertexShader (vsCode);
	m_particlePS = m_device.CreatePixelShader (psCode);
	m_particleGS = m_device.CreateGeometryShader (gsCode);
	m_particleLayout = m_device.CreateInputLayout<ParticleVertex> (vsCode);

	m_device.context ()->IASetInputLayout (m_inputlayout.get ());
	m_device.context ()->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	UpdateLamp (0.0f);

	//We have to make sure all shaders use constant buffers in the same slots!
	//Not all slots will be use by each shader
	ID3D11Buffer * vsb[] = { m_cbWorldMtx.get (),  m_cbViewMtx.get (), m_cbProjMtx.get () };
	m_device.context ()->VSSetConstantBuffers (0, 3, vsb); //Vertex Shaders - 0: worldMtx, 1: viewMtx,invViewMtx, 2: projMtx, 3: tex1Mtx, 4: tex2Mtx
	m_device.context ()->GSSetConstantBuffers (0, 1, vsb + 2); //Geometry Shaders - 0: projMtx
	ID3D11Buffer * psb[] = { m_cbSurfaceColor.get (), m_cbLightPos.get (), m_cbMapMtx.get () };
	m_device.context ()->PSSetConstantBuffers (0, 3, psb); //Pixel Shaders - 0: surfaceColor, 1: lightPos, 2: mapMtx
}

void RoomDemoShadow::UpdateCameraCB (XMMATRIX viewMtx) {
	XMVECTOR det;
	XMMATRIX invViewMtx = XMMatrixInverse (&det, viewMtx);
	XMFLOAT4X4 view[2];
	XMStoreFloat4x4 (view, viewMtx);
	XMStoreFloat4x4 (view + 1, invViewMtx);
	UpdateBuffer (m_cbViewMtx, view);
}

void RoomDemoShadow::UpdateLamp (float dt) {
	static auto time = 0.0f;
	time += dt;
	auto swing = 0.3f * XMScalarSin (XM_2PI * time / 8);
	auto rot = XM_2PI * time / 20;
	auto lamp = XMMatrixTranslation (0.0f, -0.4f, 0.0f) * XMMatrixRotationX (swing) * XMMatrixRotationY (rot) *
		XMMatrixTranslation (0.0f, 2.0f, 0.0f);

	XMStoreFloat4x4 (&m_lampMtx, lamp);

	XMFLOAT4 lightPos{ 0.0f, -0.05f, 0.0f, 1.0f };
	XMFLOAT4 lightTarget{ 0.0f, -10.0f, 0.0f, 1.0f };
	XMFLOAT4 upDir{ 1.0f, 0.0f, 0.0f, 0.0f };

	XMFLOAT4X4 texMtx;

	// TODO : 1.04 Calculate new light position in world coordinates
	XMStoreFloat4 (&lightPos, XMVector3TransformCoord (XMLoadFloat4 (&lightPos), lamp));
	XMStoreFloat4 (&lightTarget, XMVector3TransformCoord (XMLoadFloat4 (&lightTarget), lamp));
	XMStoreFloat4 (&upDir, XMVector3TransformNormal (XMLoadFloat4 (&upDir), lamp));
	UpdateBuffer (m_cbLightPos, lightPos);

	XMVECTOR det;

	// TODO : 1.01 Calculate light's view and inverted view matrix
	XMStoreFloat4x4 (&m_lightViewMtx[0], XMMatrixLookAtLH (XMLoadFloat4 (&lightPos), XMLoadFloat4 (&lightTarget), XMLoadFloat4 (&upDir)));
	XMStoreFloat4x4 (&m_lightViewMtx[1], XMMatrixInverse (&det, XMLoadFloat4x4 (&m_lightViewMtx[0])));

	// TODO : 1.03 Calculate map transform matrix
	XMStoreFloat4x4 (&texMtx, XMLoadFloat4x4 (&m_lightViewMtx[0]) * XMLoadFloat4x4 (&m_lightProjMtx) *
		XMMatrixScaling (0.5f, -0.5f, 1.0f) * XMMatrixTranslation (0.5f, 0.5f, -0.00001f));

	// TODO : 1.19 Modify map transform to fix z-fighting

	//XMStoreFloat4x4(&texMtx, XMMatrixIdentity());

	UpdateBuffer (m_cbMapMtx, texMtx);

}

void mini::gk2::RoomDemoShadow::UpdateParticles (float dt) {
	auto verts = m_particles.Update (dt, m_camera.getCameraPosition ());
	UpdateBuffer (m_vbParticles, verts);
}

void RoomDemoShadow::Update (const Clock & c) {
	double dt = c.getFrameTime ();
	HandleCameraInput (dt);
	UpdateLamp (static_cast<float>(dt));
	UpdateParticles (dt);
}

void RoomDemoShadow::SetWorldMtx (DirectX::XMFLOAT4X4 mtx) {
	UpdateBuffer (m_cbWorldMtx, mtx);
}

void mini::gk2::RoomDemoShadow::SetShaders (const dx_ptr<ID3D11VertexShader> & vs, const dx_ptr<ID3D11PixelShader> & ps) {
	m_device.context ()->VSSetShader (vs.get (), nullptr, 0);
	m_device.context ()->PSSetShader (ps.get (), nullptr, 0);
}

void mini::gk2::RoomDemoShadow::SetTextures (std::initializer_list<ID3D11ShaderResourceView *> resList, const dx_ptr<ID3D11SamplerState> & sampler) {
	m_device.context ()->PSSetShaderResources (0, resList.size (), resList.begin ());
	auto s_ptr = sampler.get ();
	m_device.context ()->PSSetSamplers (0, 1, &s_ptr);
}

void RoomDemoShadow::DrawMesh (const Mesh & m, DirectX::XMFLOAT4X4 worldMtx) {
	SetWorldMtx (worldMtx);
	m.Render (m_device.context ());
}

void RoomDemoShadow::DrawParticles () {
	//Set input layout, primitive topology, shaders, vertex buffer, and draw particles
	SetTextures ({ m_smokeTexture.get (), m_opacityTexture.get () });
	m_device.context ()->IASetInputLayout (m_particleLayout.get ());
	SetShaders (m_particleVS, m_particlePS);
	m_device.context ()->GSSetShader (m_particleGS.get (), nullptr, 0);
	m_device.context ()->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	unsigned int stride = sizeof (ParticleVertex);
	unsigned int offset = 0;
	auto vb = m_vbParticles.get ();
	m_device.context ()->IASetVertexBuffers (0, 1, &vb, &stride, &offset);
	m_device.context ()->Draw (m_particles.particlesCount (), 0);

	//Reset layout, primitive topology and geometry shader
	m_device.context ()->GSSetShader (nullptr, nullptr, 0);
	m_device.context ()->IASetInputLayout (m_inputlayout.get ());
	m_device.context ()->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void RoomDemoShadow::DrawScene () {
	for (auto & wallMtx : m_wallsMtx)
		DrawMesh (m_wall, wallMtx);
	DrawMesh (m_teapot, m_teapotMtx);

	//Draw shelf
	DrawMesh (m_box, m_boxMtx);
	//Draw lamp
	DrawMesh (m_lamp, m_lampMtx);
	//Draw chair seat
	DrawMesh (m_chairSeat, m_chairMtx);
	//Draw chairframe
	DrawMesh (m_chairBack, m_chairMtx);
	//Draw monitor
	DrawMesh (m_monitor, m_monitorMtx);
	//Draw screen
	DrawMesh (m_screen, m_monitorMtx);
	m_device.context ()->RSSetState (m_rsCullNone.get ());
	DrawMesh (m_tableTop, m_tableTopMtx);
	DrawMesh (m_tableSide, m_tableSideMtx);
	for (auto & tableLegMtx : m_tableLegsMtx)
		DrawMesh (m_tableLeg, tableLegMtx);
	m_device.context ()->RSSetState (nullptr);
}

void RoomDemoShadow::Render () {
	Base::Render ();

	// TODO : 1.13 Copy light's view/inverted view and projection matrix to constant buffers
	UpdateBuffer (m_cbProjMtx, m_lightProjMtx);
	UpdateBuffer (m_cbViewMtx, m_lightViewMtx);

	// TODO : 1.14 Set up view port of the appropriate size
	SIZE shadowMapSize;
	shadowMapSize.cx = TEXTURE_SIZE;
	shadowMapSize.cy = TEXTURE_SIZE;

	Viewport vp (shadowMapSize);
	m_device.context ()->RSSetViewports (1, &vp);

	// TODO : 1.15 Bind no render targets and the shadow map as depth buffer
	m_device.context ()->OMSetRenderTargets (0, nullptr, m_shadowDepthBuffer.get ());

	// TODO : 1.16 Clear the depth buffer
	m_device.context ()->ClearDepthStencilView (m_shadowDepthBuffer.get (), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// TODO : 1.17 Render objects and particles (w/o blending) to the shadow map using Phong shaders
	SetShaders (m_phongVS, m_phongPS);
	DrawScene ();

	ResetRenderTarget ();
	UpdateBuffer (m_cbProjMtx, m_projMtx);
	UpdateCameraCB ();

	// TODO : 1.08 Bind m_lightMap and m_shadowMap textures then draw objects and particles using light&shadow pixel shader
	SetTextures ({ m_lightMap.get (), m_shadowMap.get () });
	SetShaders (m_phongVS, m_lightShadowPS);

	DrawScene ();

	m_device.context ()->OMSetBlendState (m_bsAlpha.get (), nullptr, UINT_MAX);
	m_device.context ()->OMSetDepthStencilState (m_dssNoWrite.get (), 0);
	DrawParticles ();
	m_device.context ()->OMSetBlendState (nullptr, nullptr, UINT_MAX);
	m_device.context ()->OMSetDepthStencilState (nullptr, 0);
}