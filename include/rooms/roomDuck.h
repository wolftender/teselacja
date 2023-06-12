#pragma once
#include <random>
#include <array>

#include "dxApplication.h"
#include "mesh.h"
#include "environmentMapper.h"
#include "particleSystem.h"

using namespace DirectX;

namespace mini::gk2 {
	class RoomDuck : public DxApplication {
		public:
			using Base = DxApplication;

			explicit RoomDuck (HINSTANCE appInstance);

			struct SplinePoint {
				float x, y, z;
			};

			static inline float deboor (float b00, float b01, float b02, float b03, float t);

		protected:
			void Update (const Clock & dt) override;
			void Render () override;

		private:
			static constexpr unsigned int WATER_MAP_WIDTH = 512;
			static constexpr unsigned int WATER_MAP_HEIGHT = 512;
			static constexpr float WORLD_WIDTH = 30.0f;
			static constexpr float WORLD_HEIGHT = 30.0f;
			static constexpr float WORLD_DEPTH = 30.0f;
			static constexpr float WATER_LEVEL = -4.0f;
			static constexpr float DUCK_SCALE = 0.025f;
			static constexpr float RAIN_DELAY = 0.15f;

			dx_ptr<ID3D11Buffer> m_cbWorldMatrix;
			dx_ptr<ID3D11Buffer> m_cbViewMatrix;
			dx_ptr<ID3D11Buffer> m_cbProjectionMatrix;
			dx_ptr<ID3D11Buffer> m_cbSurfaceColor;		// pixel shader constant buffer slot 0
			dx_ptr<ID3D11Buffer> m_cbLightPos;			// pixel shader constant buffer slot 1

			Mesh m_meshTeapot;
			Mesh m_waterSurfaceMesh;
			Mesh m_meshSkybox;
			Mesh m_duckMesh;

			dx_ptr<ID3D11VertexShader> m_phongVS;
			dx_ptr<ID3D11PixelShader> m_phongPS;
			dx_ptr<ID3D11ComputeShader> m_waterCS;
			dx_ptr<ID3D11PixelShader> m_waterPS;
			dx_ptr<ID3D11VertexShader> m_waterVS;
			dx_ptr<ID3D11PixelShader> m_envPS;
			dx_ptr<ID3D11VertexShader> m_envVS;
			dx_ptr<ID3D11PixelShader> m_duckPS;
			dx_ptr<ID3D11VertexShader> m_duckVS;

			XMFLOAT4X4 m_projectionMatrix, m_waterSurfaceMatrix, m_skyboxMatrix, m_duckMatrix;
			XMFLOAT4 m_lightPos[2];

			dx_ptr<ID3D11InputLayout> m_inputlayout;
			dx_ptr<ID3D11InputLayout> m_duckInputLayout;

			dx_ptr<ID3D11Texture2D> m_waterTexture[2];
			dx_ptr<ID3D11ShaderResourceView> m_waterResourceView[2];
			dx_ptr<ID3D11UnorderedAccessView> m_waterUnorderedView[2];
			dx_ptr<ID3D11SamplerState> m_waterSamplerState;

			dx_ptr<ID3D11ShaderResourceView> m_envTexture;
			dx_ptr<ID3D11ShaderResourceView> m_duckTexture;

			int m_waterCurrent, m_waterPrev, m_numDroplets;

			std::random_device m_rd;
			std::mt19937_64 m_gen;
			std::uniform_int_distribution<> m_rain_distr;
			std::uniform_real_distribution<> m_splineDistrX, m_splineDistrY;

			std::array<SplinePoint, 4> m_spline;
			float m_duckAnimTime, m_rainTime;
			XMFLOAT3 m_duckPos, m_prevDuckPos;

		private:
			SplinePoint GenSplinePoint ();

			void m_SetNormalInputLayout ();
			void m_SetNormalTexInputLayout ();

			void m_SpawnDropletAt (int px, int py);

			void m_UpdateCameraCB (DirectX::XMMATRIX viewMtx);
			void m_UpdateCameraCB () { m_UpdateCameraCB (m_camera.getViewMatrix ()); }

			void m_SetWorldMtx (DirectX::XMFLOAT4X4 mtx);
			void m_SetSurfaceColor (DirectX::XMFLOAT4 color);
			void m_SetShaders (const dx_ptr<ID3D11VertexShader> & vs, const dx_ptr<ID3D11PixelShader> & ps);

			void m_DrawMesh (const Mesh & m, DirectX::XMFLOAT4X4 worldMtx);
	};
}