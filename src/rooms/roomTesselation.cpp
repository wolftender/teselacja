#include "rooms/roomTesselation.h"

namespace mini::gk2 {
	RoomTesselation::RoomTesselation (HINSTANCE appInstance) : DxApplication (appInstance, 1600, 900, L"teselacja"),
			m_msgHandler (m_window.getHandle ()),

			// constant buffers
			m_cbWorldMatrix (m_device.CreateConstantBuffer<XMFLOAT4X4> ()),
			m_cbProjectionMatrix (m_device.CreateConstantBuffer<XMFLOAT4X4> ()),
			m_cbViewMatrix (m_device.CreateConstantBuffer<XMFLOAT4X4, 2> ()),
			m_cbSurfaceColor (m_device.CreateConstantBuffer<XMFLOAT4> ()),
			m_cbLightPos (m_device.CreateConstantBuffer<XMFLOAT4> ()),
			m_cbTessFactor (m_device.CreateConstantBuffer<XMUINT4> ()),
			m_cbTextureOffset (m_device.CreateConstantBuffer<XMFLOAT4> ()),
			m_showWireframe (false), 
			m_preset (0),
			m_numVerticesX (PATCHES_WIDTH * 3 + 1),
			m_numVerticesY (PATCHES_WIDTH * 3 + 1),
			m_numVertices (m_numVerticesX * m_numVerticesY),
			m_numIndices (PATCHES_WIDTH * PATCHES_WIDTH * 16),
			m_controlPoints (m_numVertices),
			m_indices (m_numIndices),
			m_vertexStride (sizeof (XMFLOAT3)),
			m_tessFactor ({16U, 16U}) {

		// initialize bezier patch control points
		const float offsetX = (static_cast<float> (m_numVerticesX - 1)) / 2.0f;
		const float offsetY = (static_cast<float> (m_numVerticesY - 1)) / 2.0f;

		for (int i = 0; i < m_numVertices; ++i) {
			float u = static_cast<float> (i % m_numVerticesX);
			float v = static_cast<float> (i / m_numVerticesX);

			m_controlPoints[i] = { u - offsetX, 0.0f, v - offsetY };
		}

		// initialize indices
		for (int px = 0, i = 0; px < PATCHES_WIDTH; ++px) {
			for (int py = 0; py < PATCHES_WIDTH; ++py) {
				int bx = px * 3, by = py * 3;
				for (int k = 0; k < 16; ++k) {
					int x = bx + (k % 4), y = by + (k / 4);
					int idx = (y * m_numVerticesX) + x;

					m_indices[i++] = idx;
				}
			}
		}

		// projection matrix update
		auto s = m_window.getClientSize ();
		auto ar = static_cast<float>(s.cx) / s.cy;
		XMStoreFloat4x4 (&m_projectionMatrix, XMMatrixPerspectiveFovLH (XM_PIDIV4, ar, 0.01f, 100.0f));
		UpdateBuffer (m_cbProjectionMatrix, m_projectionMatrix);
		m_UpdateCameraCB ();

		XMStoreFloat4x4 (&m_teapotMatrix, XMMatrixIdentity ());

		// update tess factor
		UpdateBuffer (m_cbTessFactor, m_tessFactor);

		// set light positions
		m_lightPos = { 10.0f, 10.0f, 10.0f, 1.0f };

		// load meshes
		m_meshTeapot = Mesh::LoadMesh (m_device, L"resources/meshes/teapot.mesh");

		// load textures
		m_textureNormal = m_device.CreateShaderResourceView (L"resources/textures/normals.dds");
		m_textureDiffuse = m_device.CreateShaderResourceView (L"resources/textures/diffuse.dds");
		m_textureHeight = m_device.CreateShaderResourceView (L"resources/textures/height.dds");

		auto vsCode = m_device.LoadByteCode (L"bezierVS.cso");
		auto psCode = m_device.LoadByteCode (L"bezierPS.cso");
		auto hsCode = m_device.LoadByteCode (L"bezierHS.cso");
		auto dsCode = m_device.LoadByteCode (L"bezierDS.cso");

		m_shaderVS = m_device.CreateVertexShader (vsCode);
		m_shaderPS = m_device.CreatePixelShader (psCode);
		m_shaderDS = m_device.CreateDomainShader (dsCode);
		m_shaderHS = m_device.CreateHullShader (hsCode);

		m_inputlayout = m_device.CreateInputLayout (TESS_INPUT_LAYOUT, vsCode);
		m_vertexBuffer = m_device.CreateVertexBuffer<XMFLOAT3> (m_numVertices);
		m_indexBuffer = m_device.CreateIndexBuffer (m_indices);
		m_UpdateVertexBuffer ();

		// We have to make sure all shaders use constant buffers in the same slots!
		// Not all slots will be use by each shader
		ID3D11Buffer * vsb[] = { m_cbWorldMatrix.get (),  m_cbViewMatrix.get (), m_cbProjectionMatrix.get () };
		m_device.context ()->VSSetConstantBuffers (0, 3, vsb); //Vertex Shaders - 0: worldMtx, 1: viewMtx,invViewMtx, 2: projMtx

		ID3D11Buffer * dsb[] = { m_cbWorldMatrix.get (),  m_cbViewMatrix.get (), m_cbProjectionMatrix.get (), m_cbTextureOffset.get () };
		m_device.context ()->DSSetConstantBuffers (0, 4, dsb);

		ID3D11Buffer * psb[] = { m_cbSurfaceColor.get (), m_cbLightPos.get () };
		m_device.context ()->PSSetConstantBuffers (0, 2, psb); //Pixel Shaders - 0: surfaceColor, 1: lightPos[2]

		ID3D11Buffer * hsb[] = { m_cbTessFactor.get () };
		m_device.context ()->HSSetConstantBuffers (0, 1, hsb);

		// Imgui
		IMGUI_CHECKVERSION ();
		ImGui::CreateContext ();
		ImGuiIO & io = ImGui::GetIO (); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		ImGui::StyleColorsDark ();

		ImGui_ImplWin32_Init (m_window.getHandle ());
		ImGui_ImplDX11_Init (m_device.device ().get (), m_device.context ().get ());

		m_window.setMessageHandler (&m_msgHandler);

		// rasterizer states
		RasterizerDescription rd;
		rd.CullMode = D3D11_CULL_NONE;
		m_rsSolid = m_device.CreateRasterizerState (rd);

		rd.FillMode = D3D11_FILL_WIREFRAME;
		m_rsWireframe = m_device.CreateRasterizerState (rd);

		// sampler
		SamplerDescription sd;
		sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.BorderColor[0] = 0.0f;
		sd.BorderColor[1] = 0.0f;
		sd.BorderColor[2] = 0.0f;
		sd.BorderColor[3] = 1.0f;

		m_sampler = m_device.CreateSamplerState (sd);
	}

	RoomTesselation::~RoomTesselation () {
		m_window.setMessageHandler (nullptr);

		ImGui_ImplDX11_Shutdown ();
		ImGui_ImplWin32_Shutdown ();
		ImGui::DestroyContext ();
	}

	void RoomTesselation::Update (const Clock & c) {
		double dt = c.getFrameTime ();
		
		if (!ImGui::GetIO ().WantCaptureMouse) {
			HandleCameraInput (dt);
		}

		float dist = m_camera.getDistance ();
		float z = min (100.0f, max (0.01f, dist));
		float factor = -16.0f * log10f (z * 0.01f);

		unsigned int tess = static_cast<unsigned int> (floorf (factor));
		m_tessFactor = { tess * 2, tess * 2 };
	}

	void RoomTesselation::Render () {
		DxApplication::Render ();

		ResetRenderTarget ();
		m_UpdateCameraCB ();

		UpdateBuffer (m_cbTessFactor, m_tessFactor);
		UpdateBuffer (m_cbSurfaceColor, XMFLOAT4 (1.0f, 1.0f, 1.0f, 1.0f));
		UpdateBuffer (m_cbLightPos, m_lightPos);
		
		m_device.context ()->VSSetShader (m_shaderVS.get (), nullptr, 0);
		m_device.context ()->PSSetShader (m_shaderPS.get (), nullptr, 0);
		m_device.context ()->HSSetShader (m_shaderHS.get (), nullptr, 0);
		m_device.context ()->DSSetShader (m_shaderDS.get (), nullptr, 0);

		if (m_showWireframe) {
			m_device.context ()->RSSetState (m_rsWireframe.get ());
		} else {
			m_device.context ()->RSSetState (m_rsSolid.get ());
		}

		unsigned int offset = 0;
		ID3D11Buffer * b = m_vertexBuffer.get ();
		m_device.context ()->IASetInputLayout (m_inputlayout.get ());
		m_device.context ()->IASetIndexBuffer (m_indexBuffer.get (), DXGI_FORMAT_R16_UINT, 0);
		m_device.context ()->IASetVertexBuffers (0, 1, &b, &m_vertexStride, &offset);
		m_device.context ()->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST);

		// bind sampler and texture
		constexpr ID3D11ShaderResourceView * nullSrv[1] = { nullptr };
		ID3D11ShaderResourceView * psr[2] = { m_textureDiffuse.get (), m_textureNormal.get () };
		ID3D11ShaderResourceView * dsr[1] = { m_textureHeight.get () };
		auto sampler = m_sampler.get ();

		m_device.context ()->DSSetSamplers (0, 1, &sampler);
		m_device.context ()->PSSetSamplers (0, 1, &sampler);

		m_device.context ()->DSSetShaderResources (0, 1, dsr);
		m_device.context ()->PSSetShaderResources (0, 2, psr);

		for (int patchId = 0; patchId < PATCHES_WIDTH * PATCHES_WIDTH; ++patchId) {
			float patchX = static_cast<float>(patchId % PATCHES_WIDTH);
			float patchY = static_cast<float>(patchId / PATCHES_WIDTH);
			float patchTexWidth = 1.0f / static_cast<float>(PATCHES_WIDTH);
			float patchTexHeight = 1.0f / static_cast<float>(PATCHES_WIDTH);
			float patchTexOffsetX = patchTexWidth * patchX;
			float patchTexOffsetY = patchTexHeight * patchY;

			int indexStart = patchId * 16;
			int indexEnd = indexStart + 16;

			float dx = m_camera.getCameraPosition ().x - m_controlPoints[m_indices[indexStart + 7]].x;
			float dy = m_camera.getCameraPosition ().y - m_controlPoints[m_indices[indexStart + 7]].y;
			float dz = m_camera.getCameraPosition ().z - m_controlPoints[m_indices[indexStart + 7]].z;
			float dist = sqrtf(dx*dx + dy*dy + dz*dz);

			float z = min (100.0f, max (0.01f, dist));
			float factor = -16.0f * log10f (z * 0.01f);

			m_tessFactor.y = static_cast<unsigned int> (floorf (factor)) * 2;
			UpdateBuffer (m_cbTessFactor, m_tessFactor);
			UpdateBuffer (m_cbTextureOffset, XMFLOAT4 {patchTexOffsetX, patchTexOffsetY, patchTexWidth, patchTexHeight});

			m_device.context ()->DrawIndexed (16, indexStart, 0);
		}		

		m_RenderGUI ();
	}

	inline void prefixLabel (const std::string & label, float min_width) {
		float width = ImGui::CalcItemWidth ();

		if (width < min_width) {
			width = min_width;
		}

		float x = ImGui::GetCursorPosX ();
		ImGui::Text (label.c_str ());
		ImGui::SameLine ();
		ImGui::SetCursorPosX (x + width * 0.5f + ImGui::GetStyle ().ItemInnerSpacing.x);
		ImGui::SetNextItemWidth (-1);
	}

	void RoomTesselation::m_SetBezierPreset (RoomTesselation::BezierSurfacePreset preset) {
		const int nx = m_numVerticesX;
		const int ny = m_numVerticesY;
		const float offsetX = (static_cast<float> (nx - 1)) / 2.0f;
		const float offsetY = (static_cast<float> (ny - 1)) / 2.0f;

		auto at = [this](int u, int v) {
			return u * m_numVerticesX + v;
		};

		for (int u = 0; u < nx; ++u) {
			for (int v = 0; v < ny; ++v) {
				float nu = static_cast<float>(u) / static_cast<float>(nx);
				float nv = static_cast<float>(v) / static_cast<float>(ny);

				int index = at (u, v);

				switch (preset) {
					case BezierSurfacePreset::Flat:
						m_controlPoints[index].y = 0.0f;
						break;

					case BezierSurfacePreset::Hole:
					case BezierSurfacePreset::Mountain:
						{
							float mx = min (u, nx - u - 1);
							float my = min (v, ny - v - 1);

							if (u % 3 == 0 && v % 3 == 0 && 
								u > 0 && v > 0 && 
								u+1 < nx && v+1 < ny && 
								(u == ny-v-1 || v == u || ny-v-1 == nx-u-1 || nx-u-1 == v)) {
								mx -= 0.5f;
								my -= 0.5f;
							}

							float h = min (mx, my);
							m_controlPoints[index].y = h;
						}
						break;

					case BezierSurfacePreset::Sinusoid:
						float t = XM_PI * static_cast<float>(v) / 3.0f;
						m_controlPoints[index].y = 1.5f * sinf (t);
						break;
				}
			}
		}

		if (preset == BezierSurfacePreset::Hole || preset == BezierSurfacePreset::Mountain) {
			if (PATCHES_WIDTH % 2 == 0) {
				int h = nx/2;
				m_controlPoints[at(h,h)].y -= 0.5f;
			}
		}

		if (preset == BezierSurfacePreset::Hole) {
			for (auto & p : m_controlPoints) {
				p.y = -p.y;
			}
		}
	}

	void RoomTesselation::m_UpdateVertexBuffer () {
		D3D11_MAPPED_SUBRESOURCE rsc;
		m_device.context ()->Map (m_vertexBuffer.get (), 0, D3D11_MAP_WRITE_DISCARD, 0, &rsc);

		memcpy (rsc.pData, reinterpret_cast<const void*>(m_controlPoints.data ()), sizeof (XMFLOAT3) * m_numVertices);
		m_device.context ()->Unmap (m_vertexBuffer.get (), 0);
	}

	void RoomTesselation::m_RenderGUI () {
		constexpr const char * PRESETS[] = {
			"flat", "hole", "hill", "sinusoid"
		};

		ImGui_ImplDX11_NewFrame ();
		ImGui_ImplWin32_NewFrame ();
		ImGui::NewFrame ();

		ImGui::Begin ("Program Options", nullptr);
		ImGui::SetWindowSize (ImVec2 (300, 300), ImGuiCond_Once);

		// render editable settings
		prefixLabel ("Display wireframe", 250.0f);
		ImGui::Checkbox ("##wireframe", &m_showWireframe);

		// light
		prefixLabel ("Light X", 250.0f);
		ImGui::InputFloat ("##lightx", &m_lightPos.x);

		prefixLabel ("Light Y", 250.0f);
		ImGui::InputFloat ("##lighty", &m_lightPos.y);

		prefixLabel ("Light Z", 250.0f);
		ImGui::InputFloat ("##lightz", &m_lightPos.z);

		{
			int tessin = m_tessFactor.x, tessout = m_tessFactor.y;
			bool tessChange = false;

			prefixLabel ("Tess F. In.", 250.0f);
			tessChange = ImGui::InputInt ("##tessin", &tessin) || tessChange;

			prefixLabel ("Tess F. Out.", 250.0f);
			tessChange = ImGui::InputInt ("##tessout", &tessout) || tessChange;

			if (tessChange) {
				m_tessFactor.x = min (32, max (5, tessin));
				m_tessFactor.y = min (32, max (5, tessout));
			}
		}

		prefixLabel ("Preset", 250.0f);
		bool presetChanged = ImGui::Combo ("##bezierpreset", &m_preset, PRESETS, 4);

		ImGui::End ();

		ImGui::Render ();
		ImGui_ImplDX11_RenderDrawData (ImGui::GetDrawData ());

		if (presetChanged) {
			switch (m_preset) {
				case 0: m_SetBezierPreset (BezierSurfacePreset::Flat); break;
				case 1: m_SetBezierPreset (BezierSurfacePreset::Hole); break;
				case 2: m_SetBezierPreset (BezierSurfacePreset::Mountain); break;
				case 3: m_SetBezierPreset (BezierSurfacePreset::Sinusoid); break;
				default: m_SetBezierPreset (BezierSurfacePreset::Flat); break;
			}

			m_UpdateVertexBuffer ();
		}
	}

	void RoomTesselation::m_UpdateCameraCB (DirectX::XMMATRIX viewMtx) {
		XMVECTOR det;
		XMMATRIX invViewMtx = XMMatrixInverse (&det, viewMtx);
		XMFLOAT4X4 view[2];
		XMStoreFloat4x4 (view, viewMtx);
		XMStoreFloat4x4 (view + 1, invViewMtx);
		UpdateBuffer (m_cbViewMatrix, view);
	}

	void RoomTesselation::m_SetWorldMtx (DirectX::XMFLOAT4X4 mtx) {
		UpdateBuffer (m_cbWorldMatrix, mtx);
	}

	void RoomTesselation::m_SetSurfaceColor (DirectX::XMFLOAT4 color) {
		UpdateBuffer (m_cbSurfaceColor, color);
	}

	void RoomTesselation::m_SetShaders (const dx_ptr<ID3D11VertexShader> & vs, const dx_ptr<ID3D11PixelShader> & ps) {
		m_device.context ()->VSSetShader (vs.get (), nullptr, 0);
		m_device.context ()->PSSetShader (ps.get (), nullptr, 0);
	}

	void RoomTesselation::m_DrawMesh (const Mesh & m, DirectX::XMFLOAT4X4 worldMtx) {
		m_SetWorldMtx (worldMtx);
		m.Render (m_device.context ());
	}

	bool RoomTesselation::ImGuiMessageHandler::ProcessMessage (WindowMessage & m) {
		if (ImGui_ImplWin32_WndProcHandler (m_hwnd, m.message, m.wParam, m.lParam)) {
			return true;
		}

		return false;
	}
}