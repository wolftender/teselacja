#include "rooms/roomDuck.h"
#include "exceptions.h"

using namespace DirectX;

namespace mini::gk2 {
	RoomDuck::RoomDuck (HINSTANCE appInstance) : 
		DxApplication (appInstance, 1600, 900, L"Pokój"),

		// constant buffers
		m_cbWorldMatrix (m_device.CreateConstantBuffer<XMFLOAT4X4> ()),
		m_cbProjectionMatrix (m_device.CreateConstantBuffer<XMFLOAT4X4> ()),
		m_cbViewMatrix (m_device.CreateConstantBuffer<XMFLOAT4X4, 2> ()),
		m_cbSurfaceColor (m_device.CreateConstantBuffer<XMFLOAT4> ()),
		m_cbLightPos (m_device.CreateConstantBuffer<XMFLOAT4, 2> ()),

		// textures
		m_envTexture (m_device.CreateShaderResourceView (L"resources/textures/cubeMap.dds")),
		m_duckTexture (m_device.CreateShaderResourceView (L"resources/textures/ducktex.jpg")),

		// random generator
		m_gen (m_rd()),
		m_rain_distr (0, WATER_MAP_WIDTH * WATER_MAP_HEIGHT),
		m_splineDistrX (-14.0f, 14.0f),
		m_splineDistrY (-14.0f, 14.0f),
		
		// duck animation 
		m_duckAnimTime (0.0f),
		m_rainTime (0.0f),
		m_duckPos{0.0f, 0.0f, 0.0f},
		m_prevDuckPos{0.0f, 0.0f, 0.0f} {

		// projection matrix update
		auto s = m_window.getClientSize ();
		auto ar = static_cast<float>(s.cx) / s.cy;
		XMStoreFloat4x4 (&m_projectionMatrix, XMMatrixPerspectiveFovLH (XM_PIDIV4, ar, 0.01f, 100.0f));
		UpdateBuffer (m_cbProjectionMatrix, m_projectionMatrix);
		m_UpdateCameraCB ();

		XMStoreFloat4x4 (&m_waterSurfaceMatrix, XMMatrixRotationX (XM_PIDIV2) *
			XMMatrixScaling (WORLD_WIDTH, WORLD_HEIGHT, WORLD_DEPTH) * XMMatrixTranslation (0.0f, WATER_LEVEL, 0.0f));

		XMStoreFloat4x4 (&m_skyboxMatrix, XMMatrixScaling (WORLD_WIDTH, WORLD_HEIGHT, WORLD_DEPTH));
		XMStoreFloat4x4 (&m_duckMatrix, XMMatrixScaling (DUCK_SCALE, DUCK_SCALE, DUCK_SCALE));

		// set light positions
		m_lightPos[0] = { 1.0f, 1.0f, 1.0f, 1.0f };
		m_lightPos[1] = { -1.0f, -1.0f, -1.0f, 1.0f };

		// load meshes
		m_meshTeapot = Mesh::LoadMesh (m_device, L"resources/meshes/teapot.mesh");
		m_duckMesh = Mesh::LoadTexturedMesh (m_device, L"resources/meshes/duck.txt");

		m_waterSurfaceMesh = Mesh::DoubleRect (m_device, 1.0f);
		m_meshSkybox = Mesh::Skybox (m_device, 1.0f, 1.0f);

		auto vsCode = m_device.LoadByteCode (L"phongVS.cso");
		auto psCode = m_device.LoadByteCode (L"phongPS.cso");
		auto csCode = m_device.LoadByteCode (L"waterCS.cso");

		m_phongVS = m_device.CreateVertexShader (vsCode);
		m_phongPS = m_device.CreatePixelShader (psCode);

		m_waterCS = m_device.CreateComputeShader (csCode);

		vsCode = m_device.LoadByteCode (L"waterVS.cso");
		m_waterVS = m_device.CreateVertexShader (vsCode);

		psCode = m_device.LoadByteCode (L"waterPS.cso");
		m_waterPS = m_device.CreatePixelShader (psCode);

		vsCode = m_device.LoadByteCode (L"envVS.cso");
		m_envVS = m_device.CreateVertexShader (vsCode);

		psCode = m_device.LoadByteCode (L"envPS.cso");
		m_envPS = m_device.CreatePixelShader (psCode);

		m_inputlayout = m_device.CreateInputLayout (VertexPositionNormal::Layout, vsCode);
		m_SetNormalInputLayout ();

		// We have to make sure all shaders use constant buffers in the same slots!
		// Not all slots will be use by each shader
		ID3D11Buffer * vsb[] = { m_cbWorldMatrix.get (),  m_cbViewMatrix.get (), m_cbProjectionMatrix.get () };
		m_device.context ()->VSSetConstantBuffers (0, 3, vsb); //Vertex Shaders - 0: worldMtx, 1: viewMtx,invViewMtx, 2: projMtx

		ID3D11Buffer * psb[] = { m_cbSurfaceColor.get (), m_cbLightPos.get () };
		m_device.context ()->PSSetConstantBuffers (0, 2, psb); //Pixel Shaders - 0: surfaceColor, 1: lightPos[2]

		// shaders and input layout for the duck (they are different because of UVs)
		vsCode = m_device.LoadByteCode (L"duckVS.cso");
		m_duckVS = m_device.CreateVertexShader (vsCode);

		psCode = m_device.LoadByteCode (L"duckPS.cso");
		m_duckPS = m_device.CreatePixelShader (psCode);

		m_duckInputLayout = m_device.CreateInputLayout (VertexPositionNormalTex::Layout, vsCode);

		// initialize the resource views for compute shader
		Texture2DDescription textureDesc;
		textureDesc.Width = WATER_MAP_WIDTH;
		textureDesc.Height = WATER_MAP_HEIGHT;
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R32_FLOAT;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

		m_waterTexture[0] = m_device.CreateTexture (textureDesc);
		m_waterTexture[1] = m_device.CreateTexture (textureDesc);

		//////////
		ShaderResourceViewDescription srvDesc;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		m_waterResourceView[0] = m_device.CreateShaderResourceView (m_waterTexture[0], &srvDesc);
		m_waterResourceView[1] = m_device.CreateShaderResourceView (m_waterTexture[1], &srvDesc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		ZeroMemory (&uavDesc, sizeof (uavDesc));
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;

		m_waterUnorderedView[0] = m_device.CreateUnorderedAccessView (m_waterTexture[0], &uavDesc);
		m_waterUnorderedView[1] = m_device.CreateUnorderedAccessView (m_waterTexture[1], &uavDesc);

		// sampler states
		SamplerDescription sd;
		sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.BorderColor[0] = 0.0f;
		sd.BorderColor[1] = 0.0f;
		sd.BorderColor[2] = 0.0f;
		sd.BorderColor[3] = 1.0f;

		m_waterSamplerState = m_device.CreateSamplerState (sd);

		m_waterCurrent = 1;
		m_waterPrev = 0;
		m_numDroplets = 0;

		// generate initial spline points
		m_spline[0] = GenSplinePoint ();
		m_spline[1] = GenSplinePoint ();
		m_spline[2] = GenSplinePoint ();
		m_spline[3] = GenSplinePoint ();
	}

	constexpr ID3D11ShaderResourceView * nullSrv[1] = { nullptr };
	constexpr ID3D11UnorderedAccessView * nullUav[1] = { nullptr };

	inline float RoomDuck::deboor (float b00, float b01, float b02, float b03, float t) {
		float N00 = 1.0;
		float N10 = (1.0 - t) * N00;
		float N11 = t * N00;
		float N20 = ((1.0 - t) * N10) / 2.0;
		float N21 = ((1.0 + t) * N10 + (2.0 - t) * N11) / 2.0;
		float N22 = (t * N11) / 2.0;
		float N30 = ((1.0 - t) * N20) / 3.0;
		float N31 = ((2.0 + t) * N20 + (2.0 - t) * N21) / 3.0;
		float N32 = ((1.0 + t) * N21 + (3.0 - t) * N22) / 3.0;
		float N33 = (t * N22) / 3.0;

		return b00 * N30 + b01 * N31 + b02 * N32 + b03 * N33;
	}

	void RoomDuck::Update (const Clock & c) {
		std::swap (m_waterCurrent, m_waterPrev);

		/*
		 * in this step:
		 *  waterCurrent is the z_{j-1}
		 *  waterPrev is the z_{j}
		 *  after shader run waterCurrent is the z_{j+1}!!
		 */
		
		// camera options
		double dt = c.getFrameTime ();
		HandleCameraInput (dt);

		// duck animation
		{
			m_duckAnimTime += dt * 0.25f;

			if (m_duckAnimTime > 1.0f) {
				m_duckAnimTime -= floorf (m_duckAnimTime);

				m_spline[0] = m_spline[1];
				m_spline[1] = m_spline[2];
				m_spline[2] = m_spline[3];
				m_spline[3] = GenSplinePoint ();
			}

			float nx = deboor (m_spline[0].x, m_spline[1].x, m_spline[2].x, m_spline[3].x, m_duckAnimTime + 0.01f);
			float ny = deboor (m_spline[0].y, m_spline[1].y, m_spline[2].y, m_spline[3].y, m_duckAnimTime + 0.01f);

			float cx = deboor (m_spline[0].x, m_spline[1].x, m_spline[2].x, m_spline[3].x, m_duckAnimTime);
			float cy = deboor (m_spline[0].y, m_spline[1].y, m_spline[2].y, m_spline[3].y, m_duckAnimTime);
			float cz = WATER_LEVEL;

			m_prevDuckPos = m_duckPos;
			m_duckPos = { cx, cz, cy };

			float duckAngle = atan2f (cy - ny, nx - cx);

			XMStoreFloat4x4 (&m_duckMatrix,
				XMMatrixRotationY (duckAngle + XM_PI) *
				XMMatrixScaling (DUCK_SCALE, DUCK_SCALE, DUCK_SCALE) *
				XMMatrixTranslation (m_duckPos.x, m_duckPos.y, m_duckPos.z));
		}

		// generate rain droplets
		{
			m_rainTime += dt;

			if (m_rainTime > RAIN_DELAY) {
				auto rainPixel = m_rain_distr (m_gen);
				int rainPixelX = rainPixel % WATER_MAP_WIDTH;
				int rainPixelY = rainPixel / WATER_MAP_WIDTH;

				m_SpawnDropletAt (rainPixelX, rainPixelY);

				m_rainTime = 0.0f;
			}

			constexpr float HALF_WIDTH = WORLD_WIDTH / 2.0f;
			constexpr float HALF_DEPTH = WORLD_DEPTH / 2.0f;

			int duckPixelX = WATER_MAP_WIDTH * ((HALF_WIDTH + m_duckPos.x) / WORLD_WIDTH);
			int duckPixelY = WATER_MAP_HEIGHT * ((HALF_DEPTH + m_duckPos.z) / WORLD_DEPTH);
			m_SpawnDropletAt (duckPixelX, duckPixelY);

			m_numDroplets++;
		}

		// update the water texture using compute shader
		auto srv = m_waterResourceView[m_waterPrev].get ();
		auto uav = m_waterUnorderedView[m_waterCurrent].get ();

		m_device.context ()->CSSetShader (m_waterCS.get (), nullptr, 0);
		m_device.context ()->CSSetShaderResources (0, 1, &srv);
		m_device.context ()->CSSetUnorderedAccessViews (0, 1, &uav, nullptr);

		// texture is 512x512
		// thread group is 16x16
		// therefore we need 32x32 dispatch
		m_device.context ()->Dispatch (32, 32, 1);

		// this has to be done to flush the buffers
		m_device.context ()->CSSetShader (nullptr, nullptr, 0);
		m_device.context ()->CSSetShaderResources (0, 1, nullSrv);
		m_device.context ()->CSSetUnorderedAccessViews (0, 1, nullUav, nullptr);
	}

	void RoomDuck::Render () {
		DxApplication::Render ();

		ResetRenderTarget ();
		m_UpdateCameraCB ();
		m_SetNormalInputLayout ();

		m_SetShaders (m_waterVS, m_waterPS);
		UpdateBuffer (m_cbLightPos, m_lightPos);

		// set textures and sampler state for water
		ID3D11ShaderResourceView * waterSrv[2] = { m_waterResourceView[m_waterCurrent].get (), m_envTexture.get () };
		auto envSrv = m_envTexture.get ();
		auto duckSrv = m_duckTexture.get ();

		auto sampler = m_waterSamplerState.get ();

		// draw water surface
		UpdateBuffer (m_cbSurfaceColor, XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f));
		m_device.context ()->PSSetShaderResources (0, 2, waterSrv);
		m_device.context ()->PSSetSamplers (0, 1, &sampler);
		m_DrawMesh (m_waterSurfaceMesh, m_waterSurfaceMatrix);

		// draw environment cube
		m_SetShaders (m_envVS, m_envPS);
		m_device.context ()->PSSetShaderResources (0, 1, &envSrv);
		m_DrawMesh (m_meshSkybox, m_skyboxMatrix);

		// draw the duck
		m_SetNormalTexInputLayout ();
		m_SetShaders (m_duckVS, m_duckPS);
		m_device.context ()->PSSetShaderResources (0, 1, &duckSrv);
		m_DrawMesh (m_duckMesh, m_duckMatrix);

		m_device.context ()->PSSetShaderResources (0, 1, nullSrv);
	}

	RoomDuck::SplinePoint RoomDuck::GenSplinePoint () {
		SplinePoint p;
		p.x = m_splineDistrX (m_gen);
		p.y = m_splineDistrY (m_gen);
		p.z = WATER_LEVEL;

		return p;
	}

	void RoomDuck::m_SetNormalInputLayout () {
		m_device.context ()->IASetInputLayout (m_inputlayout.get ());
		m_device.context ()->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void RoomDuck::m_SetNormalTexInputLayout () {
		m_device.context ()->IASetInputLayout (m_duckInputLayout.get ());
		m_device.context ()->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void RoomDuck::m_SpawnDropletAt (int px, int py) {
		constexpr float rainData[1] = { 2.0f };
		constexpr UINT rowPitch = WATER_MAP_WIDTH * sizeof (float);
		constexpr UINT depthPitch = WATER_MAP_WIDTH * WATER_MAP_HEIGHT * sizeof (float);

		auto tex = m_waterTexture[m_waterPrev].get ();
		D3D11_BOX box;
		box.left = px;
		box.right = px + 1;
		box.top = py;
		box.bottom = py + 1;
		box.front = 0;
		box.back = 1;

		m_device.context ()->UpdateSubresource (tex, 0, &box, rainData, rowPitch, depthPitch);
	}

	void RoomDuck::m_UpdateCameraCB (DirectX::XMMATRIX viewMtx) {
		XMVECTOR det;
		XMMATRIX invViewMtx = XMMatrixInverse (&det, viewMtx);
		XMFLOAT4X4 view[2];
		XMStoreFloat4x4 (view, viewMtx);
		XMStoreFloat4x4 (view + 1, invViewMtx);
		UpdateBuffer (m_cbViewMatrix, view);
	}

	void RoomDuck::m_SetWorldMtx (DirectX::XMFLOAT4X4 mtx) {
		UpdateBuffer (m_cbWorldMatrix, mtx);
	}

	void RoomDuck::m_SetSurfaceColor (DirectX::XMFLOAT4 color) {
		UpdateBuffer (m_cbSurfaceColor, color);
	}

	void RoomDuck::m_SetShaders (const dx_ptr<ID3D11VertexShader> & vs, const dx_ptr<ID3D11PixelShader> & ps) {
		m_device.context ()->VSSetShader (vs.get (), nullptr, 0);
		m_device.context ()->PSSetShader (ps.get (), nullptr, 0);
	}

	void RoomDuck::m_DrawMesh (const Mesh & m, DirectX::XMFLOAT4X4 worldMtx) {
		m_SetWorldMtx (worldMtx);
		m.Render (m_device.context ());
	}
}