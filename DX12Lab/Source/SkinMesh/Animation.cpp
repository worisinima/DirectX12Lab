#include "Animation.h"
#include "../Common/MathHelper.h"

float AnimationTrack::GetStartTime() const
{
	return mKeyFrames.front().TimePos;
}

float AnimationTrack::GetEndTime() const
{
	return mKeyFrames.back().TimePos;
}

void AnimationTrack::Interpolate(float t, DirectX::FMathLib::Matrix& M) const
{
	if (t <= mKeyFrames.front().TimePos)
	{
		XMVECTOR S = XMLoadFloat3(&mKeyFrames.front().Scale);
		XMVECTOR P = XMLoadFloat3(&mKeyFrames.front().Translation);
		XMVECTOR Q = XMLoadFloat4(&mKeyFrames.front().RotationQuat);

		XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, zero, Q, P));
	}
	else if (t >= mKeyFrames.back().TimePos)
	{
		XMVECTOR S = XMLoadFloat3(&mKeyFrames.back().Scale);
		XMVECTOR P = XMLoadFloat3(&mKeyFrames.back().Translation);
		XMVECTOR Q = XMLoadFloat4(&mKeyFrames.back().RotationQuat);

		XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, zero, Q, P));
	}
	else
	{
		for (UINT i = 0; i < mKeyFrames.size() - 1; ++i)
		{
			if (t >= mKeyFrames[i].TimePos && t <= mKeyFrames[i + 1].TimePos)
			{
				float lerpPercent = (t - mKeyFrames[i].TimePos) / (mKeyFrames[i + 1].TimePos - mKeyFrames[i].TimePos);

				XMVECTOR s0 = XMLoadFloat3(&mKeyFrames[i].Scale);
				XMVECTOR s1 = XMLoadFloat3(&mKeyFrames[i + 1].Scale);

				XMVECTOR p0 = XMLoadFloat3(&mKeyFrames[i].Translation);
				XMVECTOR p1 = XMLoadFloat3(&mKeyFrames[i + 1].Translation);

				XMVECTOR q0 = XMLoadFloat4(&mKeyFrames[i].RotationQuat);
				XMVECTOR q1 = XMLoadFloat4(&mKeyFrames[i + 1].RotationQuat);

				XMVECTOR S = XMVectorLerp(s0, s1, lerpPercent);
				XMVECTOR P = XMVectorLerp(p0, p1, lerpPercent);
				XMVECTOR Q = XMQuaternionSlerp(q0, q1, lerpPercent);

				XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
				XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, zero, Q, P));

				break;
			}
		}
	}
}

float AnimationSequence::GetStartTime() const
{
	// Find smallest start time over all bones in this clip.
	float t = MathHelper::Infinity;
	for (UINT i = 0; i < mAnimationTracks.size(); ++i)
	{
		t = MathHelper::Min(t, mAnimationTracks[i].GetStartTime());
	}

	return t;
}

float AnimationSequence::GetEndTime() const
{
	// Find largest end time over all bones in this clip.
	float t = 0.0f;
	for (UINT i = 0; i < mAnimationTracks.size(); ++i)
	{
		t = MathHelper::Max(t, mAnimationTracks[i].GetEndTime());
	}

	return t;
}

void AnimationSequence::Interpolate(float t, std::vector<DirectX::FMathLib::Matrix>& boneTransforms)const
{
	for (UINT i = 0; i < mAnimationTracks.size(); ++i)
	{
		mAnimationTracks[i].Interpolate(t, boneTransforms[i]);
	}
}

bool AnimationSequence::IsContainTrack(const std::string& goleTrackName)
{
	for (const AnimationTrack& track : mAnimationTracks)
	{
		if (track.mTrackName == goleTrackName)
			return true;
	}
	return false;
}

int AnimationSequence::FindTrack(const std::string& goleTrackName)
{
	int TrackIndex = -1;
	for (const AnimationTrack& track : mAnimationTracks)
	{
		if (track.mTrackName == goleTrackName)
			return TrackIndex;

		TrackIndex++;
	}
	return TrackIndex;
}