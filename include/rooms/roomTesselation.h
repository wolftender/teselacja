#pragma once
#include <random>
#include <array>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "dxApplication.h"
#include "mesh.h"
#include "environmentMapper.h"
#include "particleSystem.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using namespace DirectX;

namespace mini::gk2 {
	constexpr D3D11_INPUT_ELEMENT_DESC TESS_INPUT_LAYOUT[1] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	class RoomTesselation : public DxApplication {
		public:
			enum class BezierSurfacePreset {
				Flat,
				Hole,
				Mountain,
				Sinusoid
			};

		private:
			class ImGuiMessageHandler : public IWindowMessageHandler {
				private:
					HWND m_hwnd;

				public:
					ImGuiMessageHandler (HWND hwnd) : m_hwnd(hwnd) { }
					virtual bool ProcessMessage (WindowMessage & m) override;
			};

			ImGuiMessageHandler m_msgHandler;

			dx_ptr<ID3D11Buffer> m_cbWorldMatrix;
			dx_ptr<ID3D11Buffer> m_cbViewMatrix;
			dx_ptr<ID3D11Buffer> m_cbProjectionMatrix;
			dx_ptr<ID3D11Buffer> m_cbSurfaceColor;		// pixel shader constant buffer slot 0
			dx_ptr<ID3D11Buffer> m_cbLightPos;			// pixel shader constant buffer slot 1

			dx_ptr<ID3D11Buffer> m_vertexBuffer;
			unsigned int m_vertexStride;
			unsigned int m_vertexCount;

			Mesh m_meshTeapot;

			dx_ptr<ID3D11VertexShader> m_shaderVS;
			dx_ptr<ID3D11PixelShader> m_shaderPS;
			dx_ptr<ID3D11HullShader> m_shaderHS;
			dx_ptr<ID3D11DomainShader> m_shaderDS;

			dx_ptr<ID3D11InputLayout> m_inputlayout;
			dx_ptr<ID3D11RasterizerState> m_rsSolid, m_rsWireframe;

			XMFLOAT4X4 m_projectionMatrix, m_teapotMatrix;
			XMFLOAT4 m_lightPos;

			std::vector<XMFLOAT3> m_controlPoints;

			// user interface editable variables
			bool m_showWireframe;
			int m_preset;

		public:
			using Base = DxApplication;

			explicit RoomTesselation (HINSTANCE appInstance);
			~RoomTesselation ();

		protected:
			void Update (const Clock & dt) override;
			void Render () override;

		private:
			void m_SetBezierPreset (RoomTesselation::BezierSurfacePreset preset);

			void m_UpdateVertexBuffer ();
			void m_RenderGUI ();

			void m_UpdateCameraCB (DirectX::XMMATRIX viewMtx);
			void m_UpdateCameraCB () { m_UpdateCameraCB (m_camera.getViewMatrix ()); }

			void m_SetWorldMtx (DirectX::XMFLOAT4X4 mtx);
			void m_SetSurfaceColor (DirectX::XMFLOAT4 color);
			void m_SetShaders (const dx_ptr<ID3D11VertexShader> & vs, const dx_ptr<ID3D11PixelShader> & ps);

			void m_DrawMesh (const Mesh & m, DirectX::XMFLOAT4X4 worldMtx);
	};
}