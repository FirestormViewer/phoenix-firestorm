/** 
 * @file lltoolface.cpp
 * @brief A tool to align objects
 */

#include "llviewerprecompiledheaders.h"

// File includes
#include "qtoolalign.h"

// Library includes
#include "v3math.h"

// Viewer includes
#include "llagentcamera.h"
#include "llbox.h"
#include "llcylinder.h"
#include "llfloatertools.h"
#include "llselectmgr.h"
#include "llviewercamera.h"
#include "llviewerobject.h"
#include "llviewerwindow.h"

const F32 MANIPULATOR_SIZE = 5.0f;
const F32 MANIPULATOR_SELECT_SIZE = 20.0f;
const F32 FUDGE = 0.001f;  // because of SL precision/rounding

QToolAlign::QToolAlign()
:	LLTool(std::string("Align"))
{
}

QToolAlign::~QToolAlign()
{
}

BOOL QToolAlign::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mHighlightedAxis != -1)
	{
		align();
	}
	else
	{
		gViewerWindow->pickAsync(x, y, mask, pickCallback);
	}
		
	return TRUE;
}

void QToolAlign::pickCallback(const LLPickInfo& pick_info)
{
	LLViewerObject* object = pick_info.getObject();

	if (object)
	{
		if (object->isAvatar())
		{
			return;
		}

		if (pick_info.mKeyMask & MASK_SHIFT)
		{
			// If object not selected, select it
			if ( !object->isSelected() )
			{
				LLSelectMgr::getInstance()->selectObjectAndFamily(object);
			}
			else
			{
				LLSelectMgr::getInstance()->deselectObjectAndFamily(object);
			}
		}
		else
		{
			LLSelectMgr::getInstance()->deselectAll();
			LLSelectMgr::getInstance()->selectObjectAndFamily(object);
		}
		
	}
	else
	{
		if (pick_info.mKeyMask != MASK_SHIFT)
		{
			LLSelectMgr::getInstance()->deselectAll();
		}
	}

	LLSelectMgr::getInstance()->promoteSelectionToRoot();
}

void QToolAlign::handleSelect()
{
	// no parts, please

	LL_WARNS("ToolAlign") << "in select" << LL_ENDL;
	LLSelectMgr::getInstance()->promoteSelectionToRoot();
}

void QToolAlign::handleDeselect()
{
}

BOOL QToolAlign::findSelectedManipulator(S32 x, S32 y)
{
	mHighlightedAxis = -1;
	mHighlightedDirection = 0;

	LLViewerCamera* camera = LLViewerCamera::getInstance();

	LLMatrix4 transform;
	if (LLSelectMgr::getInstance()->getSelection()->getSelectType() == SELECT_TYPE_HUD)
	{
		LLVector4 translation(mBBox.getCenterAgent());
		transform.initRotTrans(mBBox.getRotation(), translation);
		LLMatrix4 cfr(OGL_TO_CFR_ROTATION);
		transform *= cfr;
		LLMatrix4 window_scale;
		F32 zoom_level = 2.f * gAgentCamera.mHUDCurZoom;
		window_scale.initAll(LLVector3(zoom_level / camera->getAspect(), zoom_level, 0.f),
							 LLQuaternion::DEFAULT,
							 LLVector3::zero);
		transform *= window_scale;
	}
	else
	{
		transform.initAll(LLVector3(1.f, 1.f, 1.f), mBBox.getRotation(), mBBox.getCenterAgent());

		LLMatrix4 projection_matrix = camera->getProjection();
		LLMatrix4 model_matrix = camera->getModelview();

		transform *= model_matrix;
		transform *= projection_matrix;
	}


	LLRect world_view_rect = gViewerWindow->getWorldViewRectScaled();
	F32 half_width = (F32)world_view_rect.getWidth() / 2.f;
	F32 half_height = (F32)world_view_rect.getHeight() / 2.f;
	LLVector2 manip2d;
	LLVector2 mousePos((F32)x - half_width, (F32)y - half_height);
	LLVector2 delta;

	LLVector3 bbox_scale = mBBox.getMaxLocal() - mBBox.getMinLocal();
	
	for (S32 axis = VX; axis <= VZ; axis++)
	{
		for (F32 direction = -1.0f; direction <= 1.0f; direction += 2.0f)
		{
			LLVector3 axis_vector = LLVector3(0.f, 0.f, 0.f);
			axis_vector.mV[axis] = direction * bbox_scale.mV[axis] / 2.0f;
			
			LLVector4 manipulator_center = LLVector4(axis_vector);

			LLVector4 screen_center = manipulator_center * transform;
			screen_center /= screen_center.mV[VW];

			manip2d.setVec(screen_center.mV[VX] * half_width, screen_center.mV[VY] * half_height);

			delta = manip2d - mousePos;

			if (delta.magVecSquared() < MANIPULATOR_SELECT_SIZE * MANIPULATOR_SELECT_SIZE)
			{
				mHighlightedAxis = axis;
				mHighlightedDirection = direction;
				return TRUE;
			}
		}
	}

	return FALSE;
}

BOOL QToolAlign::handleHover(S32 x, S32 y, MASK mask)
{
	if (mask & MASK_SHIFT)
	{
		mForce = FALSE;
	}
	else
	{
		mForce = TRUE;
	}
	
	gViewerWindow->setCursor(UI_CURSOR_ARROW);
	return findSelectedManipulator(x, y);
}

void setup_transforms_bbox(LLBBox bbox)
{
	// translate to center
	LLVector3 center = bbox.getCenterAgent();
	gGL.translatef(center.mV[VX], center.mV[VY], center.mV[VZ]);

	// rotate
	LLQuaternion rotation = bbox.getRotation();
	F32 angle_radians, x, y, z;
	rotation.getAngleAxis(&angle_radians, &x, &y, &z);
	gGL.flush();
	gGL.rotatef(angle_radians * RAD_TO_DEG, x, y, z);

	// scale
	LLVector3 scale = bbox.getMaxLocal() - bbox.getMinLocal();
	gGL.scalef(scale.mV[VX], scale.mV[VY], scale.mV[VZ]);
}

void render_bbox(LLBBox bbox)
{
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();

	setup_transforms_bbox(bbox);

	gGL.flush();
	gBox.render();

	gGL.popMatrix();
}

void render_cone_bbox(LLBBox bbox)
{
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();

	setup_transforms_bbox(bbox);

	gGL.flush();
	gCone.render();

	gGL.popMatrix();
}

// the selection bbox isn't axis aligned, so we must construct one
// should this be cached in the selection manager?  yes.
LLBBox get_selection_axis_aligned_bbox()
{
	LLBBox selection_bbox = LLSelectMgr::getInstance()->getBBoxOfSelection();
	LLVector3 position = selection_bbox.getPositionAgent();
	
	LLBBox axis_aligned_bbox = LLBBox(position, LLQuaternion(), LLVector3(), LLVector3());
	axis_aligned_bbox.addPointLocal(LLVector3());

	// cycle over the nodes in selection
	for (LLObjectSelection::iterator selection_iter = LLSelectMgr::getInstance()->getSelection()->begin();
		 selection_iter != LLSelectMgr::getInstance()->getSelection()->end();
		 ++selection_iter)
	{
		LLSelectNode *select_node = *selection_iter;
		if (select_node)
		{
			LLViewerObject* object = select_node->getObject();
			if (object)
			{
				axis_aligned_bbox.addBBoxAgent(object->getBoundingBoxAgent());
			}
		}
	}

	
	return axis_aligned_bbox;
}

void QToolAlign::computeManipulatorSize()
{
	LLViewerCamera* camera = LLViewerCamera::getInstance();

	if (LLSelectMgr::getInstance()->getSelection()->getSelectType() == SELECT_TYPE_HUD)
	{
		mManipulatorSize = MANIPULATOR_SIZE / (camera->getViewHeightInPixels() *
											   gAgentCamera.mHUDCurZoom);
	}
	else
	{
		F32 distance = dist_vec(gAgentCamera.getCameraPositionAgent(), mBBox.getCenterAgent());

		if (distance > FUDGE)
		{
			// range != zero
			F32 fraction_of_fov = MANIPULATOR_SIZE / camera->getViewHeightInPixels();
			F32 apparent_angle = fraction_of_fov * camera->getView();  // radians
			mManipulatorSize = MANIPULATOR_SIZE * distance * tan(apparent_angle);
		}
		else
		{
			// range == zero
			mManipulatorSize = MANIPULATOR_SIZE;
		}
	}

	//Assume that UI scale factor is equivalent for X and Y axi
	F32 ui_scale_factor = LLUI::getScaleFactor().mV[VX];
	mManipulatorSize *= ui_scale_factor;
}


LLColor4 manipulator_color[3] = { LLColor4(0.7f, 0.0f, 0.0f, 0.5f),
								   LLColor4(0.0f, 0.7f, 0.0f, 0.5f),
								   LLColor4(0.0f, 0.0f, 0.7f, 0.5f) };
								   

void QToolAlign::renderManipulators()
{
	computeManipulatorSize();
	LLVector3 bbox_center = mBBox.getCenterAgent();
	LLVector3 bbox_scale = mBBox.getMaxLocal() - mBBox.getMinLocal();
	
	for (S32 axis = VX; axis <= VZ; axis++)
	{
		for (F32 direction = -1.0f; direction <= 1.0f; direction += 2.0f)
		{
			F32 size = mManipulatorSize;
			LLColor4 color = manipulator_color[axis];

			if ((axis == mHighlightedAxis) && (direction == mHighlightedDirection))
			{
				size *= 1.25f;
				color *= 1.5f;
			}

			S32 arrows = 1;
			if (mForce)
			{
				arrows = 2;
			}

			for (S32 i = 0; i < arrows; i++)
			{
				LLVector3 axis_vector = LLVector3(0.f, 0.f, 0.f);
				axis_vector.mV[axis] = direction * (bbox_scale.mV[axis] / 2.0f + i * (size / 3.0f));

				LLVector3 manipulator_center = bbox_center + axis_vector;

				LLQuaternion manipulator_rotation;
				manipulator_rotation.shortestArc(LLVector3(0.f, 0.f, 1.f), -1.0f * axis_vector);

				LLBBox manipulator_bbox = LLBBox(manipulator_center, manipulator_rotation,
					LLVector3(), LLVector3());

				manipulator_bbox.addPointLocal(LLVector3(-1.f, -1.f, -0.75f) * size * 0.5f);
				manipulator_bbox.addPointLocal(LLVector3(1.f, 1.f, 0.75f) * size * 0.5f);

				gGL.color4fv(color.mV);
				gGL.color4fv(color.mV);

				render_cone_bbox(manipulator_bbox);
			}
		}
	}
}

BOOL QToolAlign::canAffectSelection()
{
	// An selection is scalable if you are allowed to both edit and move
	// everything in it, and it does not have any sitting agents
	BOOL can_scale = LLSelectMgr::getInstance()->getSelection()->getObjectCount() != 0;
	if (can_scale)
	{
		struct f : public LLSelectedObjectFunctor
		{
			virtual bool apply(LLViewerObject* objectp)
			{
				return objectp->permModify() && objectp->permMove() && !objectp->isSeat();
			}
		} func;
		can_scale = LLSelectMgr::getInstance()->getSelection()->applyToObjects(&func);
	}
	return can_scale;
}

void QToolAlign::render()
{
	if(canAffectSelection())
	{
		mBBox = get_selection_axis_aligned_bbox();

		// Draw bounding box
		LLGLSUIDefault gls_ui;
		LLGLEnable gl_blend(GL_BLEND);
		LLGLEnable gls_alpha_test(GL_ALPHA_TEST);
		LLGLDepthTest gls_depth(GL_FALSE);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		// render box
		LLColor4 default_normal_color( 0.7f, 0.7f, 0.7f, 0.1f );
		gGL.color4fv( default_normal_color.mV );

		render_bbox(mBBox);
		renderManipulators();
	}
}

// only works for our specialized (AABB, position centered) bboxes
BOOL bbox_overlap(LLBBox bbox1, LLBBox bbox2)
{
	LLVector3 delta = bbox1.getCenterAgent() - bbox2.getCenterAgent();

	LLVector3 half_extent = (bbox1.getExtentLocal() + bbox2.getExtentLocal()) / 2.0f;

	return ((fabs(delta.mV[VX]) < half_extent.mV[VX] - FUDGE) &&
			(fabs(delta.mV[VY]) < half_extent.mV[VY] - FUDGE) &&
			(fabs(delta.mV[VZ]) < half_extent.mV[VZ] - FUDGE));
}

// used to sort bboxes before packing
class BBoxCompare
{
public:
	BBoxCompare(S32 axis, F32 direction, std::map<LLPointer<LLViewerObject>, LLBBox >& bboxes) :
		mAxis(axis), mDirection(direction), mBBoxes(bboxes) {}

	BOOL operator() (LLViewerObject* object1, LLViewerObject* object2)
	{
		LLVector3 corner1 = mBBoxes[object1].getCenterAgent() -
			mDirection * mBBoxes[object1].getExtentLocal() / 2.0f;

		LLVector3 corner2 = mBBoxes[object2].getCenterAgent() -
			mDirection * mBBoxes[object2].getExtentLocal() / 2.0f;

		
		return mDirection * corner1.mV[mAxis] < mDirection * corner2.mV[mAxis];
	}

	S32 mAxis;
	F32 mDirection;
	std::map<LLPointer<LLViewerObject>, LLBBox >& mBBoxes;
};

void QToolAlign::align()
{
	// no linkset parts, please
	LLSelectMgr::getInstance()->promoteSelectionToRoot();
	
	std::vector<LLPointer<LLViewerObject> > objects;
	std::map<LLPointer<LLViewerObject>, LLBBox > original_bboxes;
	
	// cycle over the nodes in selection and collect them into an array
	for (LLObjectSelection::root_iterator selection_iter = LLSelectMgr::getInstance()->getSelection()->root_begin();
		 selection_iter != LLSelectMgr::getInstance()->getSelection()->root_end();
		 ++selection_iter)
	{
		LLSelectNode *select_node = *selection_iter;
		if (select_node)
		{
			LLViewerObject* object = select_node->getObject();
			if (object)
			{
				LLVector3 position = object->getPositionAgent();
	
				LLBBox bbox = LLBBox(position, LLQuaternion(), LLVector3(), LLVector3());
				bbox.addPointLocal(LLVector3());

				// add the parent's bbox
				bbox.addBBoxAgent(object->getBoundingBoxAgent());
				LLViewerObject::const_child_list_t& children = object->getChildren();

				for (LLViewerObject::const_child_list_t::const_iterator it = children.begin();
					 it != children.end(); it++)
				{
					// add the child's bbox
					LLViewerObject* child = *it;
					bbox.addBBoxAgent(child->getBoundingBoxAgent());
				}
				
				objects.push_back(object);
				original_bboxes[object] = bbox;
			}
		}
	}

	S32 axis = mHighlightedAxis;
	F32 direction = mHighlightedDirection;

	// sort them into positional order for proper packing
	BBoxCompare compare(axis, direction, original_bboxes);
	sort(objects.begin(), objects.end(), compare);

	// storage for their new position after alignment - start with original position first
	std::map<LLPointer<LLViewerObject>, LLBBox > new_bboxes = original_bboxes;

	// find new positions
	for (size_t i = 0; i < objects.size(); i++)
	{
		LLBBox target_bbox = mBBox;
		LLVector3 target_corner = target_bbox.getCenterAgent() - 
			direction * target_bbox.getExtentLocal() / 2.0f;
	
		LLViewerObject* object = objects[i];

		LLBBox this_bbox = original_bboxes[object];
		LLVector3 this_corner = this_bbox.getCenterAgent() -
			direction * this_bbox.getExtentLocal() / 2.0f;

		// for packing, we cycle over several possible positions, taking the smallest that does not overlap
		F32 smallest = direction * 9999999.f;  // 999999 guarenteed not to be the smallest
		for (size_t j = 0; j <= i; j++)
		{
			// how far must it move?
			LLVector3 delta = target_corner - this_corner;

			// new position moves only on one axis, please
			LLVector3 delta_one_axis = LLVector3(0.f, 0.f, 0.f);
			delta_one_axis.mV[axis] = delta.mV[axis];
			
			LLVector3 new_position = this_bbox.getCenterAgent() + delta_one_axis;

			// construct the new bbox
			LLBBox new_bbox = LLBBox(new_position, LLQuaternion(), LLVector3(), LLVector3());
			new_bbox.addPointLocal(this_bbox.getExtentLocal() / 2.0f);
			new_bbox.addPointLocal(-1.0f * this_bbox.getExtentLocal() / 2.0f);

			// check to see if it overlaps the previously placed objects
			BOOL overlap = FALSE;

			//LL_DEBUGS("ToolAlign") << "i=" << i << " j=" << j << LL_ENDL;
			
			if (!mForce) // well, don't check if in force mode
			{
				for (size_t k = 0; k < i; k++)
				{
					LLViewerObject* other_object = objects[k];
					LLBBox other_bbox = new_bboxes[other_object];

					BOOL overlaps_this = bbox_overlap(other_bbox, new_bbox);

					//if (overlaps_this)
					//{
					//	LL_WARNS("ToolAlign") << "overlap" << new_bbox.getCenterAgent() << other_bbox.getCenterAgent() << LL_ENDL;
					//	LL_WARNS("ToolAlign") << "extent" << new_bbox.getExtentLocal() << other_bbox.getExtentLocal() << LL_ENDL;
					//}

					overlap = (overlap || overlaps_this);
				}
			}

			if (!overlap)
			{
				F32 this_value = (new_bbox.getCenterAgent() -
								  direction * new_bbox.getExtentLocal() / 2.0f).mV[axis];

				if (direction * this_value < direction * smallest)
				{
					smallest = this_value;
					// store it
					new_bboxes[object] = new_bbox;
				}
			}

			// update target for next time through the loop
			if (j < objects.size())
			{
				LLBBox next_bbox = new_bboxes[objects[j]];
				target_corner = next_bbox.getCenterAgent() +
					direction * next_bbox.getExtentLocal() / 2.0f;
			}
		}
	}

	// now move them
	for (size_t i = 0; i < objects.size(); i++)
	{
		LLViewerObject* object = objects[i];

		LLBBox original_bbox = original_bboxes[object];
		LLBBox new_bbox = new_bboxes[object];

		LLVector3 delta = new_bbox.getCenterAgent() - original_bbox.getCenterAgent();
		
		LLVector3 original_position = object->getPositionAgent();
		LLVector3 new_position = original_position + delta;

		object->setPosition(new_position);
	}
	
	
	LLSelectMgr::getInstance()->sendMultipleUpdate(UPD_POSITION);
}
