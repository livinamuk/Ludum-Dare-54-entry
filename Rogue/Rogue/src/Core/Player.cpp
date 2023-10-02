#include "Player.h"
#include "../Core/Audio.hpp"
#include "../Core/Input.h"
#include "../Core/GL.h"
#include "../Common.h"
#include "../Util.hpp"

namespace Player {
	glm::vec3 _position = glm::vec3(0);
	glm::vec3 _rotation = glm::vec3(-0.1f, -HELL_PI * 0.5f, 0);
	float _viewHeightStanding = 1.65f;
	float _viewHeightCrouching = 1.15f;
	//float _viewHeightCrouching = 3.15f; hovery
	float _crouchDownSpeed = 17.5f;
	float _currentViewHeight = _viewHeightStanding;
	float _walkingSpeed = 4;
	float _crouchingSpeed = 2;
	glm::mat4 _viewMatrix = glm::mat4(1);
	glm::mat4 _inverseViewMatrix = glm::mat4(1);
	glm::vec3 _viewPos = glm::vec3(0);
	glm::vec3 _front = glm::vec3(0);
	glm::vec3 _forward = glm::vec3(0);
	glm::vec3 _up = glm::vec3(0);
	glm::vec3 _right = glm::vec3(0);
	float _breatheAmplitude = 0.00052f;
	float _breatheFrequency = 8;
	float _headBobAmplitude = 0.00505f;
	float _headBobFrequency = 25.0f;
	bool _moving = false;
	bool _crouching = false;

	float _targetXRot = _rotation.x;
	float _targetYRot = _rotation.y;
}

void Player::Init(glm::vec3 position) {
	_position = position;
	_rotation = glm::vec3(-0.1f, -HELL_PI * 0.5f, 0);

	_targetXRot = _rotation.x;
	_targetYRot = _rotation.y;
}

void Player::Update(float deltaTime) {
	
	// Mouselook
	if (GL::WindowHasFocus()) {
		float mouseSensitivity = 0.002f;

		float cameraRotateSpeed = 50;

		if (!dead) {
			_targetXRot += -Input::GetMouseOffsetY() * mouseSensitivity;
			_targetYRot += -Input::GetMouseOffsetX() * mouseSensitivity;
		}
		else {
			_targetXRot = 1.1f;
			cameraRotateSpeed = 20;
		}
		_rotation.x = Util::FInterpTo(_rotation.x, _targetXRot, deltaTime, cameraRotateSpeed);
		_rotation.y = Util::FInterpTo(_rotation.y, _targetYRot, deltaTime, cameraRotateSpeed);
		_rotation.x = std::min(_rotation.x, 1.5f);
		_rotation.x = std::max(_rotation.x, -1.5f);
	}

	// Crouching
	_crouching = false;
	if (Input::KeyDown(HELL_KEY_LEFT_CONTROL_GLFW)) {
		_crouching = true;
	}

	// Speed
	float speed = _crouching ? _crouchingSpeed : _walkingSpeed;
	speed *= deltaTime;

	// View height
	float viewHeightTarget = _crouching ? _viewHeightCrouching : _viewHeightStanding;

	if (!dead) {
		_currentViewHeight = Util::FInterpTo(_currentViewHeight, viewHeightTarget, deltaTime, _crouchDownSpeed);
	} else {
		_currentViewHeight = Util::FInterpTo(_currentViewHeight, 1.2f, deltaTime, 22);
	}

	// Breathe bob
	static float totalTime;
	totalTime += 0.0075f;
	Transform breatheTransform;
	breatheTransform.position.x = cos(totalTime * _breatheFrequency) * _breatheAmplitude * 1;
	breatheTransform.position.y = sin(totalTime * _breatheFrequency) * _breatheAmplitude * 2;

	// Head bob
	Transform headBobTransform;
	if (_moving) {
		headBobTransform.position.x = cos(totalTime * _headBobFrequency) * _headBobAmplitude * 1;
		headBobTransform.position.y = sin(totalTime * _headBobFrequency) * _headBobAmplitude * 2;
	}

	// View matrix
	Transform camTransform;
	camTransform.position = _position + glm::vec3(0, _currentViewHeight, 0);
	camTransform.rotation = _rotation;
	_viewMatrix = glm::inverse(headBobTransform.to_mat4() * breatheTransform.to_mat4() * camTransform.to_mat4());
	_inverseViewMatrix = glm::inverse(_viewMatrix);
	_right = glm::vec3(_inverseViewMatrix[0]);
	_up = glm::vec3(_inverseViewMatrix[1]);
	_front = glm::vec3(_inverseViewMatrix[2]);// *glm::vec3(-1, -1, -1);
	_forward = glm::normalize(glm::vec3(_front.x, 0, _front.z));
	_viewPos = _inverseViewMatrix[3];

	// WSAD movement
	glm::vec3 displacement(0); 
	_moving = false;
	if (!dead) {
		if (Input::KeyDown(HELL_KEY_W)) {
			displacement -= _forward * speed;
			_moving = true;
		}
		if (Input::KeyDown(HELL_KEY_S)) {
			displacement += _forward * speed;
			_moving = true;
		}
		if (Input::KeyDown(HELL_KEY_A)) {
			displacement -= _right * speed;
			_moving = true;
		}
		if (Input::KeyDown(HELL_KEY_D)) {
			displacement += _right * speed;
			_moving = true;
		}
	}
	_position += displacement;

	if (dead) {
		_position.y = -0.5;
	}

	// Footstep audio
	static float m_footstepAudioTimer = 0;
	static float footstepAudioLoopLength = 0.3;

	if (!_moving)
		m_footstepAudioTimer = 0;
	else
	{
		if (_moving && m_footstepAudioTimer == 0) {
			int random_number = std::rand() % 4 + 1;
			std::string file = "player_step_" + std::to_string(random_number) + ".wav";
			Audio::PlayAudio(file.c_str(), 1.25f);
		}
		float timerIncrement = _crouching ? deltaTime * 0.75f : deltaTime;
		m_footstepAudioTimer += timerIncrement;

		if (m_footstepAudioTimer > footstepAudioLoopLength)
			m_footstepAudioTimer = 0;
	}
}

glm::mat4 Player::GetViewMatrix() {
	return _viewMatrix;
}

glm::mat4 Player::GetInverseViewMatrix() {
	return _inverseViewMatrix;
}

glm::vec3 Player::GetViewPos() {
	return _viewPos;
}

glm::vec3 Player::GetViewRotation() {
	return _rotation;
}


glm::vec3 Player::GetFeetPosition() {
	return _position;
}

glm::vec3 Player::GetCameraRight() {
	return _right;
}

glm::vec3 Player::GetCameraFront() {
	return _front;
}

glm::vec3 Player::GetCameraUp() {
	return _up;
}

bool Player::IsMoving() {
	return _moving;
}

bool Player::IsCrouching() {
	return _crouching;
}


void Player::EvaluateCollsions(std::vector<Vertex>& lines) { 
	for (int i = 0; i < lines.size(); i += 2) {

		glm::vec3 lineStart = lines[i].position;
		glm::vec3 lineEnd = lines[i + 1].position;
		float radius = 0.125f;// m_radius;
		glm::vec3 playerPos = _position;
		playerPos.y = 0;

		glm::vec3 closestPointOnLine = Util::ClosestPointOnLine(playerPos, lineStart, lineEnd);

		glm::vec3 dir = glm::normalize(closestPointOnLine - playerPos);
		float distToLine = glm::length(closestPointOnLine - playerPos);
		float correctionFactor = radius - distToLine;

		if (glm::length(closestPointOnLine - playerPos) < radius) {
			_position -= dir * correctionFactor;
			_position.y = 0;
			//_collisionFound = true;
			//return true;
		}
	}
}
