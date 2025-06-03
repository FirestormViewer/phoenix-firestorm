/**
 * @file fsmaniproatejoint.cpp
 * @brief custom manipulator for rotating joints
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2025 Beq Janus @ Second Life
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

 #include "llviewerprecompiledheaders.h"
 
 #include "fsmaniprotatejoint.h"

// library includes
#include "llmath.h"
#include "llgl.h"
#include "llrender.h"
#include "v4color.h"
#include "llprimitive.h"
#include "llview.h"
#include "llfontgl.h"

#include "llrendersphere.h"
#include "llvoavatar.h"
#include "lljoint.h"
#include "llagent.h"          // for gAgent, etc.
#include "llagentcamera.h"
#include "llappviewer.h"
#include "llcontrol.h"
#include "llfloaterreg.h"
#include "llresmgr.h" // for LLLocale
#include "llviewerwindow.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewershadermgr.h"
#include "fsfloaterposer.h"
// -------------------------------------


/**
 * @brief Renders a pulsing sphere at a specified joint position in the world.
 *
 * This function creates a visual indicator in the form of a pulsing sphere
 * at a given joint position. The sphere's size oscillates over time to create
 * a pulsing effect, making it easier to identify in the 3D space.
 *
 * @param joint_world_position The position of the joint in world coordinates where the sphere should be rendered.
 * @param color The color of the sphere. Defaults to white (1.0, 1.0, 1.0, 1.0).
 *
 * @return void
 *
 */
static void renderPulsingSphere(const LLVector3& joint_world_position, const LLColor4& color = LLColor4(0.f, 0.f, 1.f, 0.3f))
{
    constexpr float MAX_SPHERE_RADIUS = 0.02f;      // Base radius in agent-space units.
    constexpr float PULSE_AMPLITUDE = 0.005f;         // Additional radius variation.
    constexpr float PULSE_FREQUENCY = 1.f;         // Pulses per second.
    constexpr float PULSE_TIME_DOMAIN = 5.f;         // Keep the time input small.

    // Get the current time (in seconds) from the global timer.
    const U64 timeMicrosec = gFrameTime; 
    // Convert microseconds to seconds
    const F64 timeSec = std::fmod(static_cast<F64>(timeMicrosec) / 1000000.0, PULSE_TIME_DOMAIN);
    // Compute the pulse factor using a sine wave. This value oscillates between 0 and 1.
    float pulseFactor = 0.75f + 0.25f * std::sin(PULSE_FREQUENCY * 2.f * F_PI * static_cast<F32>(timeSec));

    // Calculate the current sphere radius.
    float currentRadius = MAX_SPHERE_RADIUS - PULSE_AMPLITUDE * pulseFactor;

    LLGLSUIDefault gls_ui;
    gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep);
    LLGLDepthTest gls_depth(GL_TRUE);
    LLGLEnable gl_blend(GL_BLEND);

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.pushMatrix();
    {

        // Translate to the joint's position
        gGL.translatef(joint_world_position.mV[VX], joint_world_position.mV[VY], joint_world_position.mV[VZ]);
        gGL.pushMatrix();
        {
            gDebugProgram.bind();

            LLGLEnable cull_face(GL_CULL_FACE);
            LLGLDepthTest gls_depth(GL_FALSE);
            gGL.pushMatrix();
            {
                gGL.color4fv(color.mV);
                gGL.diffuseColor4fv(color.mV);

                gGL.scalef(currentRadius, currentRadius, currentRadius);

                gSphere.render();
                gGL.flush();
            }
            gGL.popMatrix();

            gUIProgram.bind();
        }
        gGL.popMatrix();
    }
    gGL.popMatrix();

    // Check for OpenGL errors
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
    {
        LL_INFOS() << "OpenGL Error: " << err << LL_ENDL;
    }
}

static void renderStaticSphere(const LLVector3& joint_world_position, const LLColor4& color = LLColor4(1.f, 1.f, 0.f, .6f), float radius=0.01f)
{
    LLGLSUIDefault gls_ui;
    gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep);
    LLGLDepthTest gls_depth(GL_TRUE);
    LLGLEnable gl_blend(GL_BLEND);

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.pushMatrix();
    {

        // Translate to the joint's position
        gGL.translatef(joint_world_position.mV[VX], joint_world_position.mV[VY], joint_world_position.mV[VZ]);
        gGL.pushMatrix();
        {
            gDebugProgram.bind();

            LLGLEnable cull_face(GL_CULL_FACE);
            LLGLDepthTest gls_depth(GL_FALSE);
            gGL.pushMatrix();
            {
                gGL.color4fv(color.mV);
                gGL.diffuseColor4fv(color.mV);

                gGL.scalef(radius, radius, radius);

                gSphere.render();
                gGL.flush();
            }
            gGL.popMatrix();

            gUIProgram.bind();
        }
        gGL.popMatrix();
    }
    gGL.popMatrix();

    // Check for OpenGL errors
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
    {
        LL_INFOS() << "OpenGL Error: " << err << LL_ENDL;
    }
}


bool FSManipRotateJoint::isMouseOverJoint(S32 mouseX, S32 mouseY, const LLVector3& jointWorldPos, F32 jointRadius, F32& outDistanceFromCamera, F32& outRayDistanceFromCenter) const
{
    // LL_INFOS("FSManipRotateJoint") << "Checking mouse("<< mouseX << "," << mouseY << ") over joint at: " << jointWorldPos << LL_ENDL;
    
    auto joint_center = gAgent.getPosGlobalFromAgent( jointWorldPos );

    // centre in *agent* space
    LLVector3 agent_space_center = gAgent.getPosAgentFromGlobal(joint_center);
    LLVector3 ray_pt, ray_dir;
    LLManipRotate::mouseToRay(mouseX, mouseY, &ray_pt, &ray_dir);

    // Vector from ray origin to sphere center
    LLVector3 to_center = agent_space_center - ray_pt;
    // Project that onto the ray direction
    F32 proj_len = ray_dir * to_center;

    if (proj_len > 0.f)
    {
        // Closest approach squared = |to_center|^2 – (proj_len)^2
        F32 closest_dist_sq = to_center.magVecSquared() - (proj_len * proj_len);
        if (closest_dist_sq <= jointRadius * jointRadius)
        {
            // ray *does* hit the sphere; compute the entrance intersection distance
            F32 offset = sqrtf(jointRadius*jointRadius - closest_dist_sq);
            outDistanceFromCamera = proj_len - offset;  // distance along the ray to the front intersection            
            outRayDistanceFromCenter = offset;
            return true;
        }    
    }
    return (false);  
}

//static 
std::unordered_map<std::string, LLVector3> FSManipRotateJoint::sReferenceUpVectors = {};

//static
const std::vector<std::string_view> FSManipRotateJoint::sSelectableJoints = 
{
    // head, torso, legs
    { "mHead" },
    { "mNeck" },
    { "mPelvis" },
    { "mChest" },
    { "mTorso" },
    { "mCollarLeft" },
    { "mShoulderLeft" },
    { "mElbowLeft" },
    { "mWristLeft" },
    { "mCollarRight" },
    { "mShoulderRight" },
    { "mElbowRight" },
    { "mWristRight" },
    { "mHipLeft" },
    { "mKneeLeft" },
    { "mAnkleLeft" },
    { "mHipRight" },
    { "mKneeRight" },
    { "mAnkleRight" },

};

const std::unordered_map<FSManipRotateJoint::e_manip_part, FSManipRotateJoint::RingRenderParams> FSManipRotateJoint::sRingParams = 
{ 
    { LL_ROT_Z,   { LL_ROT_Z,   LLVector4(1.f, 1.f, SELECTED_MANIPULATOR_SCALE, 1.f), 0.f,             LLVector3(),              LLColor4(0.f,0.f,1.f,1.f), LLColor4(0.f,0.f,1.f,0.3f), 2 } },
    { LL_ROT_Y,   { LL_ROT_Y,   LLVector4(1.f, SELECTED_MANIPULATOR_SCALE, 1.f, 1.f), 90.f,            LLVector3(1.f,0.f,0.f),     LLColor4(0.f,1.f,0.f,1.f), LLColor4(0.f,1.f,0.f,0.3f), 1 } },
    { LL_ROT_X,   { LL_ROT_X,   LLVector4(SELECTED_MANIPULATOR_SCALE, 1.f, 1.f, 1.f), 90.f,            LLVector3(0.f,1.f,0.f),     LLColor4(1.f,0.f,0.f,1.f), LLColor4(1.f,0.f,0.f,0.3f), 0 } }
};
// Helper function: Builds an alignment quaternion from the computed bone axes.
// This quaternion rotates from the default coordinate system (assumed to be
// X = (1,0,0), Y = (0,1,0), Z = (0,0,1)) into the bone’s natural coordinate system.
LLQuaternion FSManipRotateJoint::computeAlignmentQuat(const BoneAxes& boneAxes) const
{
    LLQuaternion alignmentQuat(boneAxes.naturalX, boneAxes.naturalY, boneAxes.naturalZ);
    alignmentQuat.normalize();
    return alignmentQuat;
}

/**
 * @brief Computes the natural axes for the bone associated with the joint.
 *
 * This function calculates a set of orthogonal axes that represent the natural
 * orientation of the bone. It uses the joint's end point and a reference vector
 * to determine these axes, with the Z-axis along the joint/bone and the Y axis
 * perpendicular and in the plan of the world vertical, except when the joint is vertical
 * in which case the X-axis is used. You can provide a custom reference vector by setting
 * an entry in the sReferenceUpVectors map (joints like thumbs need this ideally).
 *
 * @return BoneAxes A struct containing the computed natural X, Y, and Z axes for the bone.
 */
FSManipRotateJoint::BoneAxes FSManipRotateJoint::computeBoneAxes() const
{
    BoneAxes axes;

    // Use 0,0,0 as local start for joint.
    LLVector3 joint_local_pos (0.f,0.f,0.f);

    // Transform the local endpoint (mEnd) into world space.
    LLVector3 localEnd = mJoint->getEnd();

    axes.naturalZ = localEnd - joint_local_pos;
    axes.naturalZ.normalize();

    // Choose a reference vector. We'll use world up (0,1,0) as the default,
    // but check for an override.
    LLVector3 reference(0.f, 0.f, 1.f);
    std::string jointName = mJoint->getName();
    auto iter = sReferenceUpVectors.find(jointName);
    if (iter != sReferenceUpVectors.end())
    {
        reference = iter->second;
    }

    // However, if the bone is nearly vertical relative to world up, then world up may be almost co-linear with naturalZ.
    if (std::fabs(axes.naturalZ * reference) > 0.99f)
    {
        // Use an alternate reference (+x)
        reference = LLVector3(1.f, 0.f, 0.f);
    }

    // Now, we want the naturalY to be the projection of the chosen reference onto the plane
    // between natrualZ and rference.
    //   naturalY = reference - (naturalZ dot reference)*naturalZ (I think)
    axes.naturalY = reference - (axes.naturalZ * (axes.naturalZ * reference));
    axes.naturalY.normalize();

    // Compute naturalX as the cross product of naturalY and naturalZ.
    axes.naturalX = axes.naturalY % axes.naturalZ;
    axes.naturalX.normalize();

    return axes;
}

/**
 * @brief Highlights the joint sphere that the mouse is hovering over.
 *
 * This function iterates through all "selectable joints" of the avatar and checks if the mouse
 * is hovering over any of them. It updates the highlighted joint to be the closest one to the camera
 * that the mouse is over.
 * the Selectable Joints are currently statically defined as a usable subset of all joints.
 * TODO(Beq) allow other subsets to be highlighted/selected when editing specifica areas such as face or hands.
 *
 * @param mouseX The x-coordinate of the mouse cursor in screen space.
 * @param mouseY The y-coordinate of the mouse cursor in screen space.
 *
 * @return void
 *
 * @note This function updates the mHighlightedJoint and mHighlightedPartDistance member variables.
 */
void FSManipRotateJoint::highlightHoverSpheres(S32 mouseX, S32 mouseY)
{
    // Ensure we have an avatar to work with.
    if (!mAvatar) return;
    mHighlightedJoint = nullptr; // reset the highlighted joint

    // Iterate through the avatar's joint map.
    F32 nearest_hit_distance = 0.f;
    F32 nearest_ray_distance = 0.f;
    LLJoint * nearest_joint = nullptr;
    for ( const auto& entry : getSelectableJoints())
    {
        
        LLJoint* joint = mAvatar->getJoint(std::string(entry));  
        if (!joint)
            continue;

        // Update the joint's world matrix to ensure its position is current.
        joint->updateWorldMatrixParent();
        joint->updateWorldMatrix();

        // Retrieve the joint's world position (in agent space).
        LLVector3 jointWorldPos = joint->getWorldPosition();
        LLCachedControl<F32> target_radius(gSavedSettings, "FSManipRotateJointTargetSize", 0.03f);
        F32 distance_from_camera;
        F32 distance_from_joint;
        if (isMouseOverJoint(mouseX, mouseY, jointWorldPos, target_radius, distance_from_camera, distance_from_joint) == true)
        {
            // we want to highlight the closest
            // If there is no joint or
            // this joint is a closer hit than the previous one
            if (!nearest_joint || nearest_ray_distance > distance_from_camera || 
                (nearest_ray_distance == distance_from_camera && nearest_hit_distance > distance_from_joint))
            {
                nearest_joint = joint;
                nearest_hit_distance = distance_from_joint;
                nearest_ray_distance = distance_from_camera;
            }
        }
    }
    mHighlightedJoint = nearest_joint;
}

FSManipRotateJoint::FSManipRotateJoint(LLToolComposite* composite)
: LLManipRotate(composite)
{}

// -------------------------------------

void FSManipRotateJoint::setJoint(LLJoint* joint)
{
    mJoint = joint;

    // Save initial rotation as baseline for delta rotation
    if (mJoint)
    {
        mSavedJointRot = mJoint->getWorldRotation();
        mBoneAxes = computeBoneAxes();
        mNaturalAlignmentQuat = computeAlignmentQuat(mBoneAxes);
    }
}

void FSManipRotateJoint::setAvatar(LLVOAvatar* avatar)
{
    mAvatar = avatar;
}

/**
 * @brief Handles the selection of the joint rotate tool
 *
 * This function is called when the rotate tool is selectedfor manipulation.
 *
 * @note It serves no purpose right now but might be useful once we add transform and scale
 */
void FSManipRotateJoint::handleSelect()
{
    // Not entirely sure this is needed in the current implementation.
    if (mJoint)
    {
        mSavedJointRot = mJoint->getWorldRotation();
    }
}

// We override this because we don't have a selection center from LLSelectMgr.
// Mostly copied from the base class, but without the selection center logic.
// Instead, we get the joint's position, convert to global, and store in mRotationCenter.
/**
 * @brief Updates the visibility and positioning of the joint manipulator.
 *
 * This function calculates whether the joint manipulator should be visible
 * and updates its position and scale based on the camera view and UI scale.
 * It also determines if the camera is edge-on to the manipulator's axis.
 * The "edge on" state is not used currently but is used in the base class for stepping
 *
 * @return bool Returns true if the manipulator is visible, false otherwise.
 */
bool FSManipRotateJoint::updateVisiblity()
{
    if (!mJoint)
    {
        // No joint to manipulate, not visible
        return false;
    }

    if (!hasMouseCapture())
    {
        mRotationCenter = gAgent.getPosGlobalFromAgent( mJoint->getWorldPosition() );
        mCamEdgeOn = false;
    }

    bool visible = false;

    //Assume that UI scale factor is equivalent for X and Y axis
    F32 ui_scale_factor = LLUI::getScaleFactor().mV[VX];

    const LLVector3 agent_space_center = gAgent.getPosAgentFromGlobal( mRotationCenter );    // Convert from world/agent to global

    const auto * viewer_camera = LLViewerCamera::getInstance();
    visible = viewer_camera->projectPosAgentToScreen(agent_space_center, mCenterScreen );
    if (visible)
    {
        mCenterToCam = gAgentCamera.getCameraPositionAgent() - agent_space_center;
        mCenterToCamNorm = mCenterToCam;
        mCenterToCamMag = mCenterToCamNorm.normalize();
        LLVector3 cameraAtAxis = viewer_camera->getAtAxis();
        cameraAtAxis.normalize();

        F32 z_dist = -1.f * (mCenterToCam * cameraAtAxis);

        // Don't drag manip if object too far away
        if (mCenterToCamMag > 0.001f)
        {
            F32 fraction_of_fov = RADIUS_PIXELS / static_cast<F32>(viewer_camera->getViewHeightInPixels());
            F32 apparent_angle = fraction_of_fov * viewer_camera->getView();  // radians
            mRadiusMeters = z_dist * tan(apparent_angle);
            mRadiusMeters *= ui_scale_factor;

            mCenterToProfilePlaneMag = mRadiusMeters * mRadiusMeters / mCenterToCamMag;
            mCenterToProfilePlane = -mCenterToProfilePlaneMag * mCenterToCamNorm;
        }
        else
        {
            visible = false;
        }
    }

    return visible;
}


/**
 * @brief Updates the scale of a specific manipulator part.
 *
 * This function smoothly interpolates the scale of a given manipulator part
 * towards its target scale. It uses a predefined set of ring parameters and
 * applies smooth interpolation for visual consistency.
 *
 * @param part The manipulator part to update (e.g., LL_ROT_X, LL_ROT_Y, LL_ROT_Z).
 * @param scales Reference to the LLVector4 containing current scales, which will be updated.
 *
 * @note This function modifies the 'scales' parameter in-place.
 */
void FSManipRotateJoint::updateManipulatorScale(EManipPart part, LLVector4& scales)
{
    auto iter = sRingParams.find(part);
    if (iter != sRingParams.end())
    {
        scales = lerp(scales, iter->second.targetScale, LLSmoothInterpolation::getInterpolant(MANIPULATOR_SCALE_HALF_LIFE));
    }
}

// Render a single ring using the given parameters and pass index (for multi-pass rendering).
void FSManipRotateJoint::renderRingPass(const RingRenderParams& params, float radius, float width, int pass)
{
    gGL.pushMatrix();
    {
    // If an extra rotation is specified, apply it.
    if (params.extraRotateAngle != 0.f)
    {
        gGL.rotatef(params.extraRotateAngle, params.extraRotateAxis.mV[0],
                    params.extraRotateAxis.mV[1], params.extraRotateAxis.mV[2]);
    }        
        // Get the appropriate scale value from mManipulatorScales.
        float scaleVal = 1.f;
        switch (params.scaleIndex)
        {
            case 0: scaleVal = mManipulatorScales.mV[VX]; break;
            case 1: scaleVal = mManipulatorScales.mV[VY]; break;
            case 2: scaleVal = mManipulatorScales.mV[VZ]; break;
            case 3: scaleVal = mManipulatorScales.mV[VW]; break;
            default: break;
        }
        gGL.scalef(scaleVal, scaleVal, scaleVal);
        gl_ring(radius, width, params.primaryColor, params.secondaryColor, CIRCLE_STEPS, pass);
    }
    gGL.popMatrix();
}

/**
 * @brief Renders the manipulator rings for joint rotation.
 *
 * This function renders the manipulator rings used for rotating joints. It handles
 * the rendering of the center sphere and individual rings based on the current
 * manipulation state and highlighted parts.
 *
 * @param agent_space_center The center point of the manipulator in agent space.
 * @param rotation The rotation to be applied to the manipulator rings.
 *
 * @return void
 */
void FSManipRotateJoint::renderManipulatorRings(const LLVector3& agent_space_center, const LLQuaternion& rotation)
{
    F32 width_meters = WIDTH_PIXELS * mRadiusMeters / RADIUS_PIXELS;
    // Translate to the joint's position
    const auto joint_world_position = mJoint->getWorldPosition();
    gDebugProgram.bind();
    gGL.pushMatrix();
    {
        LLGLEnable cull_face(GL_CULL_FACE);
        LLGLDepthTest gls_depth(GL_FALSE);        
        LLGLEnable clip_plane0(GL_CLIP_PLANE0);
        gGL.translatef(joint_world_position.mV[VX], joint_world_position.mV[VY], joint_world_position.mV[VZ]);

        LLMatrix4 rot_mat(rotation);
        gGL.multMatrix((GLfloat*)rot_mat.mMatrix);

        for (int pass = 0; pass < 2; ++pass)
        {
            if( mManipPart == LL_NO_PART || mManipPart == LL_ROT_ROLL || mHighlightedPart == LL_ROT_ROLL)
            {
                renderCenterSphere( mRadiusMeters);
                for (auto& ring_params : sRingParams)
                {
                    const auto part = ring_params.first;
                    const auto& params = ring_params.second;
                    if (mHighlightedPart == part)
                    {
                        updateManipulatorScale(part, mManipulatorScales);
                    }
                    renderRingPass(params, mRadiusMeters, width_meters, pass);
                }
                if( mManipPart == LL_ROT_ROLL || mHighlightedPart == LL_ROT_ROLL)
                {
                    static const auto roll_color = LLColor4(1.f,0.65f,0.0f,0.4f);
                    updateManipulatorScale(mManipPart, mManipulatorScales);
                    gGL.pushMatrix();
                    {
                        // Cancel the rotation applied earlier:
                        LLMatrix4 inv_rot_mat(rotation);
                        inv_rot_mat.invert();
                        gGL.multMatrix((GLfloat*)inv_rot_mat.mMatrix);                    
                        renderCenterCircle( mRadiusMeters*1.2f, roll_color, roll_color );
                    }
                    gGL.popMatrix();
                }
            }
            else
            {
                auto iter = sRingParams.find(mManipPart);
                if (iter != sRingParams.end())
                {
                    updateManipulatorScale(mManipPart, mManipulatorScales);
                    renderRingPass(iter->second, mRadiusMeters, width_meters, pass);
                }
            }
        }
    }
    gGL.popMatrix();
    gUIProgram.bind();
}

void FSManipRotateJoint::renderCenterCircle(const F32 radius, const LLColor4& normal_color, const LLColor4& highlight_color)
{
    gGL.pushMatrix();
    {
        LLGLEnable cull_face(GL_CULL_FACE);
        LLGLDepthTest gls_depth(GL_FALSE);

        constexpr int segments = 64;
        glLineWidth(6.0f); // Set the desired line thickness

        // Compute a scale factor that already factors in the radius.
        float scale = radius;
        scale *= (mManipulatorScales.mV[VX] + mManipulatorScales.mV[VY] +
                  mManipulatorScales.mV[VZ] + mManipulatorScales.mV[VW]) / 4.0f;
        gGL.diffuseColor4fv(normal_color.mV);
        gGL.scalef(scale, scale, scale);

        // Rotate the unit circle so its normal (0,0,1) aligns with mCenterToCamNorm.
        LLVector3 defaultNormal(0.f, 0.f, 1.f);
        LLVector3 targetNormal = mCenterToCamNorm;
        targetNormal.normalize();  // Ensure it is normalized.

        LLVector3 rotationAxis = defaultNormal % targetNormal;  // Cross product.
        F32 dot = defaultNormal * targetNormal;                 // Dot product.
        F32 angle = acosf(dot) * RAD_TO_DEG;                      // Convert to degrees.

        if (rotationAxis.magVec() > 0.001f)
        {
            gGL.rotatef(angle, rotationAxis.mV[VX], rotationAxis.mV[VY], rotationAxis.mV[VZ]);
        }

        // Draw a unit circle in the XY plane (which is now rotated correctly).
        gGL.begin(LLRender::LINE_LOOP);
        for (int i = 0; i < segments; i++)
        {
            float theta = 2.0f * 3.14159f * i / segments;
            // Use a unit circle here.
            LLVector3 offset(cosf(theta), sinf(theta), 0.f);
            gGL.vertex3fv(offset.mV);
        }
        gGL.end();

        glLineWidth(1.0f); // Reset the line width.
    }
    gGL.popMatrix();
}

void FSManipRotateJoint::renderCenterSphere(const F32 radius, const LLColor4& normal_color, const LLColor4& highlight_color)
{
    gGL.pushMatrix();
    {
        LLGLEnable cull_face(GL_CULL_FACE);
        LLGLDepthTest gls_depth(GL_FALSE);
        
        float scale = radius * 0.8f;
        
        if (mManipPart == LL_ROT_GENERAL || mHighlightedPart == LL_ROT_GENERAL)
        {
            mManipulatorScales = lerp(mManipulatorScales, LLVector4(1.f, 1.f, 1.f, SELECTED_MANIPULATOR_SCALE), LLSmoothInterpolation::getInterpolant(MANIPULATOR_SCALE_HALF_LIFE));
            gGL.diffuseColor4fv(highlight_color.mV);
            scale *= mManipulatorScales.mV[VW];
        }
        else
        {
            // no part selected, just a semi transp white sphere
            gGL.diffuseColor4fv(normal_color.mV);
            // Use an average of the manipulator scales when no specific part is selected
            scale *= (mManipulatorScales.mV[VX] + mManipulatorScales.mV[VY] + mManipulatorScales.mV[VZ] + mManipulatorScales.mV[VW]) / 4.0f;
        }
        
        gGL.scalef(scale, scale, scale);
        gSphere.render();

        gGL.flush();    
    }
    gGL.popMatrix();
}


/**
 * @brief Renders the joint rotation manipulator and associated visual elements.
 *
 * This function is responsible for rendering the joint rotation manipulator,
 * including the manipulator rings, axes, and debug information. It handles
 * the visibility checks, GL state setup, and calls to specific rendering
 * functions for different components of the manipulator.
 *
 * The function performs the following main tasks:
 * 1. Checks for the presence of a valid joint and avatar.
 * 2. Updates the visibility and rotation center.
 * 3. Sets up the GL state for rendering.
 * 4. Renders a pulsing sphere for highlighted joints (if applicable).
 * 5. Updates joint world matrices.
 * 6. Computes the active rotation based on user settings.
 * 7. Renders the manipulator axes and rings.
 * 8. Displays debug information (Euler angles, including delta of the active drag).
 *
 * This function does not take any parameters and does not return a value.
 * It operates on the internal state of the FSManipRotateJoint object.
 */
void FSManipRotateJoint::render()
{
    // Early-out if no joint or avatar.
    if (!mJoint || !mAvatar)
    {
        return;
    }
    
    // update visibility and rotation center.
    bool activeJointVisible = updateVisiblity();
    // Setup GL state.
    LLGLSUIDefault gls_ui;
    gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep);
    LLGLDepthTest gls_depth(GL_TRUE);
    LLGLEnable gl_blend(GL_BLEND);
    
    // Iterate through the avatar's joint map.
    // If a joint other than the currently selected is highlighted, render a pulsing sphere.
    // otherwise a small static sphere
    for (const auto& entry : getSelectableJoints())
    {
        LLJoint* joint = mAvatar->getJoint(std::string(entry));  
        if (!joint)
            continue;
        // Update the joint's world matrix to ensure its position is current.
        joint->updateWorldMatrixParent();
        joint->updateWorldMatrix();

        if( joint == mHighlightedJoint && joint != mJoint )
        {
            renderPulsingSphere(joint->getWorldPosition());
        }
        else if( joint != mJoint )
        {
            // Render a static sphere for the joint being manipulated.
            LLCachedControl<bool> show_joint_markers(gSavedSettings, "FSManipShowJointMarkers", true);                
            if(show_joint_markers)
            {
                renderStaticSphere(joint->getWorldPosition(), LLColor4(1.f, 0.5f, 0.f, 0.5f), 0.01f);
            }
        }
    }

    if (!activeJointVisible)
    {
        return;
    }

    // Update joint world matrices.
    mJoint->updateWorldMatrixParent();
    mJoint->updateWorldMatrix();
    
    const LLQuaternion joint_world_rotation = mJoint->getWorldRotation();

    const LLQuaternion parentWorldRot = (mJoint->getParent()) ? mJoint->getParent()->getWorldRotation() : LLQuaternion::DEFAULT;

    LLQuaternion currentLocalRot = mJoint->getRotation();
    
    LLQuaternion rotatedNaturalAlignment = mNaturalAlignmentQuat * currentLocalRot;
    // Compute the final world alignment:
    LLQuaternion final_world_alignment = rotatedNaturalAlignment * parentWorldRot;

    const LLVector3 agent_space_center = gAgent.getPosAgentFromGlobal(mRotationCenter);

    LLCachedControl<bool> use_natural_direction(gSavedSettings, "FSManipRotateJointUseNaturalDirection", true);    
    LLQuaternion active_rotation = use_natural_direction? final_world_alignment : joint_world_rotation;
    active_rotation.normalize();
    // Render the manipulator rings in a separate function.
    gGL.matrixMode(LLRender::MM_MODELVIEW);
    renderAxes(agent_space_center, mRadiusMeters * 1.5f, active_rotation);
    renderManipulatorRings(agent_space_center, active_rotation);

    // Debug: render joint's Euler angles for diagnostic purposes.
    renderNameXYZ(active_rotation);
}

void FSManipRotateJoint::renderAxes(const LLVector3& agent_space_center, F32 size, const LLQuaternion& rotation)
{
    LLGLEnable cull_face(GL_CULL_FACE);
    LLGLEnable clip_plane0(GL_CLIP_PLANE0);
    LLGLDepthTest gls_depth(GL_FALSE);    
    gGL.pushMatrix();
    gGL.translatef(agent_space_center.mV[VX], agent_space_center.mV[VY], agent_space_center.mV[VZ]);
    
    LLMatrix4 rot_mat(rotation);

    gGL.multMatrix((GLfloat*)rot_mat.mMatrix);

    gGL.begin(LLRender::LINES);
    
    // X-axis (Red)
    gGL.color4f(1.0f, 0.0f, 0.0f, 1.0f);
    gGL.vertex3f(-size, 0.0f, 0.0f);
    gGL.vertex3f(size, 0.0f, 0.0f);

    // Y-axis (Green)
    gGL.color4f(0.0f, 1.0f, 0.0f, 1.0f);
    gGL.vertex3f(0.0f, -size, 0.0f);
    gGL.vertex3f(0.0f, size, 0.0f);

    // Z-axis (Blue)
    gGL.color4f(0.0f, 0.0f, 1.0f, 1.0f);
    gGL.vertex3f(0.0f, 0.0f, -size);
    gGL.vertex3f(0.0f, 0.0f, size);

    gGL.end();

    gGL.popMatrix();
}

//static
std::string FSManipRotateJoint::getManipPartString(EManipPart part)
{
    switch (part)
    {
        case LL_NO_PART:        return "None";
        case LL_X_ARROW:        return "X Arrow";
        case LL_Y_ARROW:        return "Y Arrow";
        case LL_Z_ARROW:        return "Z Arrow";
        case LL_YZ_PLANE:       return "YZ Plane";
        case LL_XZ_PLANE:       return "XZ Plane";
        case LL_XY_PLANE:       return "XY Plane";
        case LL_CORNER_NNN:     return "Corner ---";
        case LL_CORNER_NNP:     return "Corner --+";
        case LL_CORNER_NPN:     return "Corner -+-";
        case LL_CORNER_NPP:     return "Corner -++";
        case LL_CORNER_PNN:     return "Corner +--";
        case LL_CORNER_PNP:     return "Corner +-+";
        case LL_CORNER_PPN:     return "Corner ++-";
        case LL_CORNER_PPP:     return "Corner +++";
        case LL_FACE_POSZ:      return "Face +Z";
        case LL_FACE_POSX:      return "Face +X";
        case LL_FACE_POSY:      return "Face +Y";
        case LL_FACE_NEGX:      return "Face -X";
        case LL_FACE_NEGY:      return "Face -Y";
        case LL_FACE_NEGZ:      return "Face -Z";
        case LL_EDGE_NEGX_NEGY: return "Edge -X-Y";
        case LL_EDGE_NEGX_POSY: return "Edge -X+Y";
        case LL_EDGE_POSX_NEGY: return "Edge +X-Y";
        case LL_EDGE_POSX_POSY: return "Edge +X+Y";
        case LL_EDGE_NEGY_NEGZ: return "Edge -Y-Z";
        case LL_EDGE_NEGY_POSZ: return "Edge -Y+Z";
        case LL_EDGE_POSY_NEGZ: return "Edge +Y-Z";
        case LL_EDGE_POSY_POSZ: return "Edge +Y+Z";
        case LL_EDGE_NEGZ_NEGX: return "Edge -Z-X";
        case LL_EDGE_NEGZ_POSX: return "Edge -Z+X";
        case LL_EDGE_POSZ_NEGX: return "Edge +Z-X";
        case LL_EDGE_POSZ_POSX: return "Edge +Z+X";
        case LL_ROT_GENERAL:    return "Rotate General";
        case LL_ROT_X:          return "Rotate X";
        case LL_ROT_Y:          return "Rotate Y";
        case LL_ROT_Z:          return "Rotate Z";
        case LL_ROT_ROLL:       return "Rotate Roll";
        default:                return "Unknown";
    }
}

/**
 * @brief Renders the XYZ coordinates and additional information as text overlay on the screen.
 *
 * This function displays the X, Y, and Z coordinates of the given vector, along with the delta angle,
 * joint name, and manipulation part. It creates a semi-transparent background and renders the text
 * with shadow effects for better visibility.
 *
 * @param vec The LLVector3 containing the X, Y, and Z coordinates to be displayed.
 *
 * @return void
 *
 * @note This function assumes the existence of class member variables such as mLastAngle, mJoint, and mManipPart.
 *       It also uses global functions and objects like gViewerWindow, LLUI, and LLFontGL.
 */
void FSManipRotateJoint::renderNameXYZ(const LLQuaternion& rot)
{
    constexpr S32 PAD = 10;
    S32 window_center_x = gViewerWindow->getWorldViewRectScaled().getWidth() / 2;
    S32 window_center_y = gViewerWindow->getWorldViewRectScaled().getHeight() / 2;
    S32 vertical_offset = window_center_y - VERTICAL_OFFSET;

    LLVector3 euler_angles;
    rot.getEulerAngles(&euler_angles.mV[0],
                                        &euler_angles.mV[1],
                                        &euler_angles.mV[2]);
    euler_angles *= RAD_TO_DEG;
    for (S32 i = 0; i < 3; ++i)
    {
        // Ensure angles are in the range [0, 360) and rounded to 0.05f
        euler_angles.mV[i] = ll_round(fmodf(euler_angles.mV[i] + 360.f, 360.f), 0.05f);
        F32 rawDelta = euler_angles.mV[i] - mLastEuler.mV[i];
        if      (rawDelta >  180.f) rawDelta -= 360.f;
        else if (rawDelta < -180.f) rawDelta += 360.f;
        mLastEuler[i] += rawDelta;
    }

    gGL.pushMatrix();
    {
        LLUIImagePtr imagep = LLUI::getUIImage("Rounded_Square");
        gViewerWindow->setup2DRender();
        const LLVector2& display_scale = gViewerWindow->getDisplayScale();
        gGL.color4f(0.f, 0.f, 0.f, 0.7f);

        imagep->draw(
            (S32)((window_center_x - 150) * display_scale.mV[VX]),
            (S32)((window_center_y + vertical_offset - PAD) * display_scale.mV[VY]),
            (S32)(340 * display_scale.mV[VX]),
            (S32)((PAD * 2 + 10) * display_scale.mV[VY] * 2),
            LLColor4(0.f, 0.f, 0.f, 0.7f) 
        );

        LLFontGL* font = LLFontGL::getFontSansSerif();
        LLLocale locale(LLLocale::USER_LOCALE);
        LLGLDepthTest gls_depth(GL_FALSE);

        auto renderTextWithShadow = [&](const std::string& text, F32 x, F32 y, const LLColor4& color) {
            font->render(utf8str_to_wstring(text), 0, x + 1.f, y - 2.f, LLColor4::black,
                LLFontGL::LEFT, LLFontGL::BASELINE,
                LLFontGL::NORMAL, LLFontGL::NO_SHADOW, S32_MAX, 1000, nullptr);
            font->render(utf8str_to_wstring(text), 0, x, y, color,
                LLFontGL::LEFT, LLFontGL::BASELINE,
                LLFontGL::NORMAL, LLFontGL::NO_SHADOW, S32_MAX, 1000, nullptr);
        };

        F32 base_y = (F32)(window_center_y + vertical_offset);
        renderTextWithShadow(llformat("X: %.3f", mLastEuler.mV[VX]), window_center_x - 122.f, base_y, LLColor4(1.f, 0.5f, 0.5f, 1.f));
        renderTextWithShadow(llformat("Y: %.3f", mLastEuler.mV[VY]), window_center_x - 47.f, base_y, LLColor4(0.5f, 1.f, 0.5f, 1.f));
        renderTextWithShadow(llformat("Z: %.3f", mLastEuler.mV[VZ]), window_center_x + 28.f, base_y, LLColor4(0.5f, 0.5f, 1.f, 1.f));
        renderTextWithShadow(llformat("⟳: %.3f", mLastAngle * RAD_TO_DEG), window_center_x + 103.f, base_y, LLColor4(1.f, 0.65f, 0.f, 1.f));
        base_y += 20.f;
        renderTextWithShadow(llformat("Joint: %s", mJoint->getName().c_str()), window_center_x - 130.f, base_y, LLColor4(1.f, 0.1f, 1.f, 1.f));
        renderTextWithShadow(llformat("Manip: %s%c", getManipPartString(mManipPart).c_str(), mCamEdgeOn?'*':' '), window_center_x + 30.f, base_y, LLColor4(1.f, 1.f, .1f, 1.f));
        if (mManipPart != LL_NO_PART)
        {
            LL_INFOS("FSManipRotateJoint") << "Joint: " << mJoint->getName()
                << ", Manip: " << getManipPartString(mManipPart)
                << ", Quaternion: " << rot
                << ", Euler Angles: " << mLastEuler
                << ", Delta Angle: " << mLastAngle * RAD_TO_DEG
                << LL_ENDL;
        }
    }
    gGL.popMatrix();

    gViewerWindow->setup3DRender();
}

void FSManipRotateJoint::renderActiveRing( F32 radius, F32 width, const LLColor4& front_color, const LLColor4& back_color)
{
    LLGLEnable cull_face(GL_CULL_FACE);
    {
        gl_ring(radius, width, back_color, back_color * 0.5f, CIRCLE_STEPS, false);
        gl_ring(radius, width, back_color, back_color * 0.5f, CIRCLE_STEPS, true);
    }
    {
        LLGLDepthTest gls_depth(GL_FALSE);
        gl_ring(radius, width, front_color, front_color * 0.5f, CIRCLE_STEPS, false);
        gl_ring(radius, width, front_color, front_color * 0.5f, CIRCLE_STEPS, true);
    }
}


// -------------------------------------
// Overriding because the base uses mObjectSelection->getFirstMoveableObject(true)
// Not sure we use it though...TBC (see mouse down on part instead)
bool FSManipRotateJoint::handleMouseDown(S32 x, S32 y, MASK mask)
{
    if (!mJoint)
    {
        return false;
    }

    // Highlight the manipulator as before.
    highlightManipulators(x, y);

    if (mHighlightedPart != LL_NO_PART)
    {
        mManipPart = (EManipPart)mHighlightedPart;

        // Get the joint's center in agent space.
        LLVector3 agent_space_center = gAgent.getPosAgentFromGlobal(mRotationCenter);

        // Use the existing function to get the intersection point.
        LLVector3 intersection = intersectMouseWithSphere(x, y, agent_space_center, mRadiusMeters);

        // Check if the returned intersection is valid.
        if (intersection.isExactlyZero())
        {
            // Treat this as a "raycast miss" and do not capture the mouse.
            return false;
        }
        else
        {
            // Save the valid intersection point.
            mInitialIntersection = intersection;
            // Also store the joint's current rotation.
            mSavedJointRot = mJoint->getWorldRotation();

            // Capture the mouse for dragging.
            setMouseCapture(true);
            return true;
        }
    }
    return false;
}

/**
 * @brief Handles the mouse down event on a manipulator part.
 *
 * Along with render, this is the main top-level entry. 
 * This function determines which manipulator part (ring/axis) is under the mouse cursor
 * using the highlightManipulator() function and highlights the selectable joints. 
 * It then saves the joint's current world rotation as the basis for the drag operation 
 * and sets the appropriate manipulation part.
 * Depending on the manipulation part, it either performs an unconstrained rotation
 * or a constrained rotation based on the axis.
 *
 * @param x The x-coordinate of the mouse cursor.
 * @param y The y-coordinate of the mouse cursor.
 * @param mask The mask indicating the state of modifier keys.
 * @return true if the mouse down event is handled successfully, false otherwise.
 */
bool FSManipRotateJoint::handleMouseDownOnPart(S32 x, S32 y, MASK mask)
{
    auto * poser = dynamic_cast<FSFloaterPoser*>(LLFloaterReg::findInstance("fs_poser"));
    // Determine which ring (axis) is under the mouse, also highlights selectable joints.
    highlightManipulators(x, y);
    // For joint manipulation, require both a valid joint and avatar.
    if (!mJoint || !mAvatar || !poser)
    {
        return false;
    }
    poser->setFocus(true);
    S32 hit_part = mHighlightedPart;

    // Save the joint’s current world rotation as the basis for the drag.
    mSavedJointRot = mJoint->getWorldRotation();

    mManipPart = (EManipPart)hit_part;

    // Convert rotation center from global to agent space.
    LLVector3 agent_space_center = gAgent.getPosAgentFromGlobal(mRotationCenter);

    // based on mManipPArt (set in highlightmanipulators). decide whether we are constrained or not in the rotation
    if (mManipPart == LL_ROT_GENERAL)
    {
        // Unconstrained rotation. we use the intersection point as the mouse down point.
        mMouseDown = intersectMouseWithSphere(x, y, agent_space_center, mRadiusMeters);
        mInitialIntersection = mMouseDown;  // Save the initial sphere intersection.
    }
    else
    {
        // Constrained rotation.
        LLVector3 axis = setConstraintAxis(); // set the axis based on the manipulator part

        mLastEuler = LLVector3::zero;

        F32 axis_onto_cam = llabs(axis * mCenterToCamNorm);
        if (axis_onto_cam < AXIS_ONTO_CAM_TOLERANCE)
        {
            LLVector3 up_from_axis = mCenterToCamNorm % axis;
            up_from_axis.normalize();
            LLVector3 cur_intersection;
            getMousePointOnPlaneAgent(cur_intersection, x, y, agent_space_center, mCenterToCam);
            cur_intersection -= agent_space_center;
            mMouseDown = projected_vec(cur_intersection, up_from_axis);
            F32 mouse_depth = SNAP_GUIDE_INNER_RADIUS * mRadiusMeters;
            F32 mouse_dist_sqrd = mMouseDown.magVecSquared();
            if (mouse_dist_sqrd > 0.0001f)
            {
                mouse_depth = sqrtf((SNAP_GUIDE_INNER_RADIUS * mRadiusMeters) *
                                    (SNAP_GUIDE_INNER_RADIUS * mRadiusMeters) - mouse_dist_sqrd);
            }
            LLVector3 projected_center_to_cam = mCenterToCamNorm - projected_vec(mCenterToCamNorm, axis);
            mMouseDown += mouse_depth * projected_center_to_cam;
            mCamEdgeOn = true; // We are in edge mode, so we can use the mouse depth.
        }
        else
        {
            mMouseDown = findNearestPointOnRing(x, y, agent_space_center, axis) - agent_space_center;
            mMouseDown.normalize();
            mCamEdgeOn = false; // Not in edge mode, so we don't use the mouse depth.
        }
        mInitialIntersection = mMouseDown;
    }

    // Set the current mouse vector equal to the initial one.
    mMouseCur = mMouseDown;

    // Save the agent’s “at” axis (this might be used later in drag calculations).
    mAgentSelfAtAxis = gAgent.getAtAxis();

    // Capture the mouse so that subsequent mouse drag events are routed here.
    setMouseCapture(true);

    // (Optionally, reset any help text timer or related UI feedback.)
    mHelpTextTimer.reset();
    sNumTimesHelpTextShown++;

    return true;
}

// We use mouseUp to update the UI, updating it during the drag is too slow.
bool FSManipRotateJoint::handleMouseUp(S32 x, S32 y, MASK mask)
{
    
    auto * poser = dynamic_cast<FSFloaterPoser*>(LLFloaterReg::findInstance("fs_poser"));
    if (hasMouseCapture())
    {   
        // Update the UI, by causing it to read back the position of the selected joints and aply those relative to the base rot
        if (poser && mJoint)
        {
            poser->updatePosedBones(mJoint->getName());
        } 
        
        // Release mouse
        setMouseCapture(false);
        mManipPart = LL_NO_PART;
        mLastAngle = 0.0f;  
        mCamEdgeOn = false;
        return true;
    }
    else if(mHighlightedJoint)
    {
        if (poser)
        {
            poser->selectJointByName(mHighlightedJoint->getName());
        }
        return true;
    }
    return false;
}


/**
 * @brief Does all the hard work of working out what inworld control we are interacting with
 * 
 * There's quite a bit of overlap with the base class. 
 * Sadly the base is built around object selection, so we need to override.
 * We also take this opportunity to highlight nearby joints that we might want to manipulate.
 * 
 * @param x mouse x-coordinate
 * @param y mouse y-coordinate
 */
void FSManipRotateJoint::highlightManipulators(S32 x, S32 y)
{
    // Clear any previous highlight.
    mHighlightedPart = LL_NO_PART;
    // Instead of using mObjectSelection->getFirstMoveableObject(),
    // simply require that the joint (and the avatar) is valid.
    if (!mJoint || !mAvatar)
    {
        highlightHoverSpheres(x, y);
        gViewerWindow->setCursor(UI_CURSOR_ARROW);
        return;
    }

    // Decide which rotation to use based on a user toggle.
    LLCachedControl<bool> use_natural_direction(gSavedSettings, "FSManipRotateJointUseNaturalDirection", true);
    // Compute the rotation center in agent space.
    LLVector3 agent_space_rotation_center = gAgent.getPosAgentFromGlobal(mRotationCenter);

    // Update joint world matrices.
    mJoint->updateWorldMatrixParent();
    mJoint->updateWorldMatrix();
    
    const LLQuaternion joint_world_rotation = mJoint->getWorldRotation();

    const LLQuaternion parentWorldRot = (mJoint->getParent()) ? mJoint->getParent()->getWorldRotation() : LLQuaternion::DEFAULT;

    LLQuaternion currentLocalRot = mJoint->getRotation();
    
    LLQuaternion rotatedNaturalAlignment = mNaturalAlignmentQuat * currentLocalRot;
    rotatedNaturalAlignment.normalize();
    // Compute the final world alignment:
    LLQuaternion final_world_alignment = rotatedNaturalAlignment * parentWorldRot;
    final_world_alignment.normalize();


    LLQuaternion joint_rot = use_natural_direction ? final_world_alignment : joint_world_rotation;

    // Compute the three local axes in world space.
    LLVector3 rot_x_axis = LLVector3::x_axis * joint_rot;
    LLVector3 rot_y_axis = LLVector3::y_axis * joint_rot;
    LLVector3 rot_z_axis = LLVector3::z_axis * joint_rot;

    // mCenterToCamNorm is assumed to be computed already (for example in updateVisibility)
    F32 proj_rot_x_axis = llabs(rot_x_axis * mCenterToCamNorm);
    F32 proj_rot_y_axis = llabs(rot_y_axis * mCenterToCamNorm);
    F32 proj_rot_z_axis = llabs(rot_z_axis * mCenterToCamNorm);

    // Variables to help choose the best candidate.
    F32 min_select_distance = 0.f;
    F32 cur_select_distance = 0.f;

    // These vectors will hold the intersection points on planes defined by each axis.
    LLVector3 mouse_dir_x, mouse_dir_y, mouse_dir_z, intersection_roll;

    // For each axis, compute the mouse intersection on a plane passing through the rotation center.
    getMousePointOnPlaneAgent(mouse_dir_x, x, y, agent_space_rotation_center, rot_x_axis);
    mouse_dir_x -= agent_space_rotation_center;
    mouse_dir_x *= 1.f + (1.f - llabs(rot_x_axis * mCenterToCamNorm)) * 0.1f;

    getMousePointOnPlaneAgent(mouse_dir_y, x, y, agent_space_rotation_center, rot_y_axis);
    mouse_dir_y -= agent_space_rotation_center;
    mouse_dir_y *= 1.f + (1.f - llabs(rot_y_axis * mCenterToCamNorm)) * 0.1f;

    getMousePointOnPlaneAgent(mouse_dir_z, x, y, agent_space_rotation_center, rot_z_axis);
    mouse_dir_z -= agent_space_rotation_center;
    mouse_dir_z *= 1.f + (1.f - llabs(rot_z_axis * mCenterToCamNorm)) * 0.1f;

    // For roll, intersect with a plane defined by the camera’s direction.
    getMousePointOnPlaneAgent(intersection_roll, x, y, agent_space_rotation_center, mCenterToCamNorm);
    intersection_roll -= agent_space_rotation_center;

    // Compute the distances (in agent-space) from the rotation center.
    F32 dist_x = mouse_dir_x.normalize();
    F32 dist_y = mouse_dir_y.normalize();
    F32 dist_z = mouse_dir_z.normalize();

    // Compute a threshold for selection.
    F32 distance_threshold = (MAX_MANIP_SELECT_DISTANCE * mRadiusMeters) / gViewerWindow->getWorldViewHeightScaled();

    // Define a lambda to test an axis ring. This captures variables by reference.
    auto testAxisRing = [&](F32 dist, const LLVector3& mouse_dir, F32 proj_factor, LLManip::e_manip_part ringId) {
        F32 absDiff = llabs(dist - mRadiusMeters);
        // Instead of multiplying by proj_factor, we divide the threshold by it,
        // so that near edge-on views (small proj_factor) yield a larger tolerance.
        if (absDiff < distance_threshold / llmax(0.05f, proj_factor))
        {
            F32 cur_select_distance = dist * (mouse_dir * mCenterToCamNorm);
            if (cur_select_distance >= -0.05f && (min_select_distance == 0.f || cur_select_distance > min_select_distance))
            {
                min_select_distance = cur_select_distance;
                mHighlightedPart = ringId;
            }
        }
    };

    // Use the lambda for each axis.
    testAxisRing(dist_x, mouse_dir_x, proj_rot_x_axis, LL_ROT_X);
    testAxisRing(dist_y, mouse_dir_y, proj_rot_y_axis, LL_ROT_Y);
    testAxisRing(dist_z, mouse_dir_z, proj_rot_z_axis, LL_ROT_Z);

    // --- Additional tests for edge-on intersections ---
    if (proj_rot_x_axis < 0.05f)
    {
        if ((proj_rot_y_axis > 0.05f && (dist_y * llabs(mouse_dir_y * rot_x_axis) < distance_threshold) && dist_y < mRadiusMeters) ||
            (proj_rot_z_axis > 0.05f && (dist_z * llabs(mouse_dir_z * rot_x_axis) < distance_threshold) && dist_z < mRadiusMeters))
        {
            mHighlightedPart = LL_ROT_X;
        }
    }
    if (proj_rot_y_axis < 0.05f)
    {
        if ((proj_rot_x_axis > 0.05f && (dist_x * llabs(mouse_dir_x * rot_y_axis) < distance_threshold) && dist_x < mRadiusMeters) ||
            (proj_rot_z_axis > 0.05f && (dist_z * llabs(mouse_dir_z * rot_y_axis) < distance_threshold) && dist_z < mRadiusMeters))
        {
            mHighlightedPart = LL_ROT_Y;
        }
    }
    if (proj_rot_z_axis < 0.05f)
    {
        if ((proj_rot_x_axis > 0.05f && (dist_x * llabs(mouse_dir_x * rot_z_axis) < distance_threshold) && dist_x < mRadiusMeters) ||
            (proj_rot_y_axis > 0.05f && (dist_y * llabs(mouse_dir_y * rot_z_axis) < distance_threshold) && dist_y < mRadiusMeters))
        {
            mHighlightedPart = LL_ROT_Z;
        }
    }

    // --- Test for roll if no primary axis was highlighted ---
    if (mHighlightedPart == LL_NO_PART)
    {
        F32 roll_distance = intersection_roll.magVec();
        F32 width_meters = WIDTH_PIXELS * mRadiusMeters / RADIUS_PIXELS;

        if (llabs(roll_distance - (mRadiusMeters + (width_meters * 2.f))) < distance_threshold * 2.f)
        {
            mHighlightedPart = LL_ROT_ROLL;
        }
        else if (roll_distance < mRadiusMeters)
        {
            mHighlightedPart = LL_ROT_GENERAL;
        }
    }

    // If nothing else of interest then test for nearby joints we can select.
    if (mHighlightedPart == LL_NO_PART)
    {
        highlightHoverSpheres(x, y);
        gViewerWindow->setCursor(UI_CURSOR_ARROW);
    }
    else
    {
        gViewerWindow->setCursor(UI_CURSOR_TOOLROTATE);
    }
}


// -------------------------------------
bool FSManipRotateJoint::handleHover(S32 x, S32 y, MASK mask)
{
    // If we are dragging (hasMouseCapture),
    // we do the "drag" logic but apply rotation to the joint
    if (hasMouseCapture() && mJoint)
    {
        drag(x, y);  // calls dragConstrained() or dragUnconstrained()
        // but in drag(), we must override it so the final rotation
        // is applied to the joint instead of an LLViewerObject
        gViewerWindow->setCursor(UI_CURSOR_TOOLROTATE);
    }
    else
    {
        highlightManipulators(x, y);
    }
    return true;
}

LLQuaternion FSManipRotateJoint::dragUnconstrained(S32 x, S32 y)
{
    // Get the camera position and the joint’s pivot (in agent space)
    LLVector3 cam = gAgentCamera.getCameraPositionAgent();
    LLVector3 agent_space_center = gAgent.getPosAgentFromGlobal(mRotationCenter);

    // Compute the current intersection on the sphere.
    mMouseCur = intersectMouseWithSphere(x, y, agent_space_center, mRadiusMeters);

    // Use the screen center (set in updateVisibility) to compute how far
    // the mouse is from the sphere’s center in screen space.
    F32 delta_x = (F32)(mCenterScreen.mX - x);
    F32 delta_y = (F32)(mCenterScreen.mY - y);
    F32 dist_from_sphere_center = sqrtf(delta_x * delta_x + delta_y * delta_y);

    // Compute a rotation axis from the stored initial intersection to the current intersection.
    LLVector3 axis = mInitialIntersection % mMouseCur;
    F32 angle = atan2f(sqrtf(axis * axis), mInitialIntersection * mMouseCur);
    axis.normalize();
    LLQuaternion sphere_rot(angle, axis);

    // If there is negligible change, return the identity.
    if (is_approx_zero(1.f - mInitialIntersection * mMouseCur))
    {
        return LLQuaternion::DEFAULT;
    }
    // If the mouse is still near the center of the manipulator in screen space,
    // simply return the computed sphere rotation.
    else if (dist_from_sphere_center < RADIUS_PIXELS)
    {
        return sphere_rot;
    }
    else
    {
        // Otherwise, compute an “extra” rotation based on a projection onto a profile plane.
        LLVector3 intersection;
        // Use the previously computed mCenterToProfilePlane and mCenterToCamNorm.
        // This computes a point on the plane defined by (center + mCenterToProfilePlane) and oriented by mCenterToCamNorm.
        getMousePointOnPlaneAgent(intersection, x, y, agent_space_center + mCenterToProfilePlane, mCenterToCamNorm);

        // Determine the “in-sphere” angle that corresponds to dragging from centre to periphery.
        F32 in_sphere_angle = F_PI_BY_TWO;
        F32 dist_to_tangent_point = mRadiusMeters;
        if (!is_approx_zero(mCenterToProfilePlaneMag))
        {
            dist_to_tangent_point = sqrtf(mRadiusMeters * mRadiusMeters - mCenterToProfilePlaneMag * mCenterToProfilePlaneMag);
            in_sphere_angle = atan2f(dist_to_tangent_point, mCenterToProfilePlaneMag);
        }

        LLVector3 profile_center_to_intersection = intersection - (agent_space_center + mCenterToProfilePlane);
        F32 dist_to_intersection = profile_center_to_intersection.normalize();
        F32 extra_angle = (-1.f + dist_to_intersection / dist_to_tangent_point) * in_sphere_angle;

        // Compute a rotation axis from the camera-to-center vector and the profile difference.
        axis = (cam - agent_space_center) % profile_center_to_intersection;
        axis.normalize();

        // Multiply the unconstrained sphere rotation with the extra rotation.
        return sphere_rot * LLQuaternion(extra_angle, axis);
    }
}

static LLQuaternion extractTwist(const LLQuaternion& rot, const LLVector3& axis)
{
    // Copy and normalise the input (defensive)
    LLQuaternion qnorm = rot;
    qnorm.normalize();

    // Extract vector part and scalar part
    LLVector3 v(qnorm.mQ[VX], qnorm.mQ[VY], qnorm.mQ[VZ]);
    F32       w = qnorm.mQ[VW];

    // Project v onto the axis (removing any perpendicular component)
    F32        dot  = v * axis;           
    LLVector3  proj = axis * dot; // proj is now purely along 'axis'

    // Build the “twist” quaternion from (proj, w), then renormalize
    LLQuaternion twist(proj.mV[VX],
                       proj.mV[VY],
                       proj.mV[VZ],
                       w);
    if (w < 0.f)
    {   
        twist = -twist;
    }
    twist.normalize();
    return twist;
}
LLQuaternion FSManipRotateJoint::dragConstrained(S32 x, S32 y)
{
    // Get the constraint axis from our joint manipulator.
    LLVector3 constraint_axis = getConstraintAxis();
    LLVector3 agent_space_center = gAgent.getPosAgentFromGlobal(mRotationCenter);
    if (mCamEdgeOn)
    {
        LLQuaternion freeRot = dragUnconstrained(x, y);
        return extractTwist(freeRot, constraint_axis);
    }
    // Project the current mouse position onto the plane defined by the constraint axis.
    LLVector3 projected_mouse;
    bool hit = getMousePointOnPlaneAgent(projected_mouse, x, y, agent_space_center, constraint_axis);
    if (!hit)
    {
        return LLQuaternion::DEFAULT;
    }
    projected_mouse -= agent_space_center;
    projected_mouse.normalize();

    // Similarly, project the initial intersection (stored at mouse down) onto the same plane.
    LLVector3 initial_proj = mInitialIntersection;
    initial_proj -= (initial_proj * constraint_axis) * constraint_axis;
    initial_proj.normalize();

    // Compute the signed angle using atan2.
    // The numerator is the magnitude of the cross product projected along the constraint axis.
    float numerator = (initial_proj % projected_mouse) * constraint_axis;
    // The denominator is the dot product.
    float denominator = initial_proj * projected_mouse;
    float angle = atan2(numerator, denominator); // angle in (-pi, pi)
    mLastAngle = angle;
    return LLQuaternion(angle, constraint_axis);
}

void FSManipRotateJoint::drag(S32 x, S32 y)
{
    if (!updateVisiblity() || !mJoint) return;

    LLQuaternion delta_rot;
    if (mManipPart == LL_ROT_GENERAL)
    {
        delta_rot = dragUnconstrained(x, y);
    }
    else
    {
        delta_rot = dragConstrained(x, y);
    }
    
    // Compose the saved joint rotation with the delta to compute the new world rotation.
    LLQuaternion new_world_rot = mSavedJointRot * delta_rot;
    mJoint->setWorldRotation(new_world_rot);
}

// set mConstrainedAxis based on mManipParat and returns it too. 
LLVector3 FSManipRotateJoint::setConstraintAxis()
{
    LLVector3 axis;
    if (mManipPart == LL_ROT_ROLL)
    {
        axis = mCenterToCamNorm;
    }
    else
    {
        // For constrained rotations about X, Y, or Z:
        // Assume mManipPart is defined such that LL_ROT_X, LL_ROT_Y, LL_ROT_Z correspond to 0, 1, 2.
        S32 axis_dir = mManipPart - LL_ROT_X;
        axis.setZero();
        if (axis_dir >= LL_NO_PART && axis_dir < LL_Z_ARROW)
        {
            axis.mV[axis_dir] = 1.f;
        }
        else
        {
            axis.mV[0] = 1.f; // Fallback to X.
        }
        // Transform the local axis into world space using the joint's world rotation.
        if (mJoint)
        {
            LLCachedControl<bool> use_natural_direction(gSavedSettings, "FSManipRotateJointUseNaturalDirection", true);    
            LLQuaternion active_rotation;
            if (use_natural_direction)
            {            
                // Get the joint's current local rotation.
                LLQuaternion currentLocalRot = mJoint->getRotation();
                const LLQuaternion parentWorldRot = (mJoint->getParent()) ? mJoint->getParent()->getWorldRotation() : LLQuaternion::DEFAULT;
                LLQuaternion rotatedNaturalAlignment = mNaturalAlignmentQuat * currentLocalRot;
                rotatedNaturalAlignment.normalize();
                LLQuaternion final_world_alignment = rotatedNaturalAlignment * parentWorldRot;
                final_world_alignment.normalize();
                active_rotation = final_world_alignment;
            }
            else
            {
                active_rotation = mJoint->getWorldRotation();
            }
            axis = axis * active_rotation;
            axis.normalize();
        }
    }
    mConstraintAxis = axis;
    return axis;
}
