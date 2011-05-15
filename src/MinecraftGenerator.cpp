#include "MinecraftGenerator.h"
#include <cv.h>
#include <highgui.h>
#include <math.h>

extern xn::UserGenerator g_UserGenerator;
extern xn::DepthGenerator g_DepthGenerator;

// Working under the assumption the arrays have the same dimension
void XnToCV(const XnRGB24Pixel *input, cv::Mat *output)
{
    memcpy(output->data,input,sizeof(XnRGB24Pixel)*output->rows*output->cols);
}

XnPoint3D PointForJoint(XnUserID user, XnSkeletonJoint joint)
{
    XnSkeletonJointPosition jointPos;
    g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(user, joint, jointPos);
    XnPoint3D pt = jointPos.position;
    
    printf("%f %f %f confidence %f\n",jointPos.position.X, jointPos.position.Y, jointPos.position.Z, jointPos.fConfidence);

	g_DepthGenerator.ConvertRealWorldToProjective(1, &pt, &pt);
	
	return pt;
}

void CopyBodyPart(cv::Mat *part, cv::Mat *skin, cv::Point2i position)
{
    for(int y = 0; y < part->rows; y++) {
        unsigned char *irow = skin->ptr<unsigned char>(y+position.y);
        unsigned char *orow = part->ptr<unsigned char>(y);
        memcpy(irow+(position.x*3), orow, part->cols*3);
    }
}

void GetLimb(XnUserID user, cv::Mat *body, cv::Mat *skin, XnSkeletonJoint joint1, XnSkeletonJoint joint2, int w, cv::Size size, cv::Point2i pos)
{
    XnPoint3D p1 = PointForJoint(user, joint1);
    XnPoint3D p2 = PointForJoint(user, joint2);
    
    float dx = p1.X-p2.X;
    float dy = p1.Y-p2.Y;
    float l = sqrt(dx*dx+dy*dy);
    dx /= l;
    dy /= l;
    
    cv::Point2f cameraPoints[] = {cv::Point2f(p1.X+(w/2)*dy, p1.Y-(w/2)*dx), cv::Point2f(p1.X-(w/2)*dy, p1.Y+(w/2)*dx),
                                  cv::Point2f(p2.X+(w/2)*dy, p2.Y-(w/2)*dx), cv::Point2f(p2.X-(w/2)*dy, p2.Y+(w/2)*dx)};
    
    cv::Point2f skinPoints[] = {cv::Point2f(0, 0), cv::Point2f(size.width-1, 0),
                                cv::Point2f(0, size.height-1), cv::Point2f(size.width-1, size.height-1)};
    
    cv::Mat transform = cv::getPerspectiveTransform(cameraPoints, skinPoints);
    cv::Mat transformed = cv::Mat(size, CV_8UC3);
    cv::warpPerspective(*body, transformed, transform, size);
    
    CopyBodyPart(&transformed, skin, pos);
}

void GetEnd(XnUserID user, cv::Mat *body, cv::Mat *skin, XnSkeletonJoint joint, cv::Point2i pos)
{
    XnPoint3D p = PointForJoint(user, joint);
    int s = 2;
    
    cv::Point2f cameraPoints[] = {cv::Point2f(p.X-s, p.Y-s), cv::Point2f(p.X+s, p.Y-s), cv::Point2f(p.X-s, p.Y+s), cv::Point2f(p.X+s, p.Y+s)};
    cv::Point2f skinPoints[] = {cv::Point2f(0, 0), cv::Point2f(3, 0), cv::Point2f(0, 3), cv::Point2f(3, 3)};
    
    cv::Size size = cv::Size(4, 4);
    cv::Mat transform = cv::getPerspectiveTransform(cameraPoints, skinPoints);
    cv::Mat transformed = cv::Mat(size, CV_8UC3);
    cv::warpPerspective(*body, transformed, transform, size);
    
    CopyBodyPart(&transformed, skin, pos);
}

void GetTorso(XnUserID user, cv::Mat *body, cv::Mat *skin)
{
    XnPoint3D ls = PointForJoint(user, XN_SKEL_LEFT_SHOULDER);
    XnPoint3D rs = PointForJoint(user, XN_SKEL_RIGHT_SHOULDER);
    XnPoint3D lh = PointForJoint(user, XN_SKEL_LEFT_HIP);
    XnPoint3D rh = PointForJoint(user, XN_SKEL_RIGHT_HIP);
    
    printf("(%f,%f,%f) (%f, %f, %f) (%f, %f, %f) (%f, %f, %f)\n", ls.X, ls.Y, ls.Z, rs.X, rs.Y, rs.Z, lh.X, lh.Y, lh.Z, rh.X, rh.Y, rh.Z);
    
    cv::Point2f cameraPoints[] = {cv::Point2f(ls.X, ls.Y), cv::Point2f(rs.X, rs.Y), cv::Point2f(lh.X, lh.Y), cv::Point2f(rh.X, rh.Y)};
    cv::Point2f skinPoints[] = {cv::Point2f(7, 0), cv::Point2f(0, 0), cv::Point2f(7, 11), cv::Point2f(0, 11)};
    
    cv::Size size = cv::Size(8, 12);
    cv::Mat transform = cv::getPerspectiveTransform(cameraPoints, skinPoints);
    
    cv::Mat transformed = cv::Mat(size, CV_8UC3);
    cv::warpPerspective(*body, transformed, transform, size);

    CopyBodyPart(&transformed, skin, cv::Point2i(20, 20));
    CopyBodyPart(&transformed, skin, cv::Point2i(32, 20));
}

void GenerateSkin(XnUserID user, cv::Mat *body, cv::Mat *skin)
{
    // Torso and sides
    GetTorso(user, body, skin);
    GetLimb(user, body, skin, XN_SKEL_RIGHT_SHOULDER, XN_SKEL_RIGHT_HIP, 6, cv::Size(4,8), cv::Point2i(16,20));
    GetLimb(user, body, skin, XN_SKEL_LEFT_SHOULDER, XN_SKEL_LEFT_HIP, 6, cv::Size(4,8), cv::Point2i(28,20));
    GetLimb(user, body, skin, XN_SKEL_RIGHT_SHOULDER, XN_SKEL_LEFT_SHOULDER, 6, cv::Size(8,4), cv::Point2i(20,16));
    GetLimb(user, body, skin, XN_SKEL_RIGHT_HIP, XN_SKEL_LEFT_HIP, 6, cv::Size(8,4), cv::Point2i(28,16));
    
    // Arms,  use different widths for some slight texture differences
    GetLimb(user, body, skin, XN_SKEL_RIGHT_SHOULDER, XN_SKEL_RIGHT_ELBOW, 7, cv::Size(4,6), cv::Point2i(40,20));
    GetLimb(user, body, skin, XN_SKEL_RIGHT_ELBOW, XN_SKEL_RIGHT_HAND, 7, cv::Size(4,6), cv::Point2i(40,26));
    GetLimb(user, body, skin, XN_SKEL_LEFT_SHOULDER, XN_SKEL_LEFT_ELBOW, 8, cv::Size(4,6), cv::Point2i(44,20));
    GetLimb(user, body, skin, XN_SKEL_LEFT_ELBOW, XN_SKEL_LEFT_HAND, 8, cv::Size(4,6), cv::Point2i(44,26));
    GetLimb(user, body, skin, XN_SKEL_RIGHT_SHOULDER, XN_SKEL_RIGHT_ELBOW, 8, cv::Size(4,6), cv::Point2i(48,20));
    GetLimb(user, body, skin, XN_SKEL_RIGHT_ELBOW, XN_SKEL_RIGHT_HAND, 8, cv::Size(4,6), cv::Point2i(48,26));
    GetLimb(user, body, skin, XN_SKEL_LEFT_SHOULDER, XN_SKEL_LEFT_ELBOW, 7, cv::Size(4,6), cv::Point2i(52,20));
    GetLimb(user, body, skin, XN_SKEL_LEFT_ELBOW, XN_SKEL_LEFT_HAND, 7, cv::Size(4,6), cv::Point2i(52,26));
    GetEnd(user, body, skin, XN_SKEL_RIGHT_SHOULDER, cv::Point2i(44,16));
    GetEnd(user, body, skin, XN_SKEL_RIGHT_HAND, cv::Point2i(48,16));
    
    // Legs, also use various widths
    GetLimb(user, body, skin, XN_SKEL_RIGHT_HIP, XN_SKEL_RIGHT_KNEE, 7, cv::Size(4,6), cv::Point2i(0,20));
    GetLimb(user, body, skin, XN_SKEL_RIGHT_KNEE, XN_SKEL_RIGHT_FOOT, 7, cv::Size(4,6), cv::Point2i(0,26));
    GetLimb(user, body, skin, XN_SKEL_LEFT_HIP, XN_SKEL_LEFT_KNEE, 8, cv::Size(4,6), cv::Point2i(4,20));
    GetLimb(user, body, skin, XN_SKEL_LEFT_KNEE, XN_SKEL_LEFT_FOOT, 8, cv::Size(4,6), cv::Point2i(4,26));
    GetLimb(user, body, skin, XN_SKEL_RIGHT_HIP, XN_SKEL_RIGHT_KNEE, 8, cv::Size(4,6), cv::Point2i(8,20));
    GetLimb(user, body, skin, XN_SKEL_RIGHT_KNEE, XN_SKEL_RIGHT_FOOT, 8, cv::Size(4,6), cv::Point2i(8,26));
    GetLimb(user, body, skin, XN_SKEL_LEFT_HIP, XN_SKEL_LEFT_KNEE, 7, cv::Size(4,6), cv::Point2i(12,20));
    GetLimb(user, body, skin, XN_SKEL_LEFT_KNEE, XN_SKEL_LEFT_FOOT, 7, cv::Size(4,6), cv::Point2i(12,26));
    GetEnd(user, body, skin, XN_SKEL_RIGHT_HIP, cv::Point2i(4,16));
    GetEnd(user, body, skin, XN_SKEL_RIGHT_FOOT, cv::Point2i(8,16));
}

void GenerateMinecraftCharacter(const xn::DepthMetaData& dmd, const xn::SceneMetaData& smd, const XnRGB24Pixel* image)
{
    int xRes = dmd.XRes();
    int yRes = dmd.YRes();
    
    cv::Mat inputImage = cv::Mat(yRes, xRes, CV_8UC3);
    cv::Mat skin = cv::Mat::zeros(cv::Size(64,32), CV_8UC3);
    XnToCV(image,&inputImage);
    cv::cvtColor(inputImage,inputImage,CV_RGB2BGR);
    cv::imwrite("blah.png",inputImage);
    
    XnUserID aUsers[15];
	XnUInt16 nUsers = 15;
	g_UserGenerator.GetUsers(aUsers, nUsers);
	int i = 0;
	for (i = 0; i < nUsers; ++i) {
	    if (g_UserGenerator.GetSkeletonCap().IsTracking(aUsers[i])) break;
	}
	
	// No users being tracked
	if (i == nUsers) return;
	
	GenerateSkin(aUsers[i], &inputImage, &skin);
	cv::imwrite("skin.png",skin);
}
