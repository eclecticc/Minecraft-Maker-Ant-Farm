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

// Getting zero points sometimes, so don't use them
int PointIsValid(XnPoint3D point)
{
    return (point.X != 0.0) && (point.Y != 0.0) && (point.Z != 0.0);
}

XnPoint3D PointForJoint(XnUserID user, XnSkeletonJoint joint)
{
    XnSkeletonJointPosition jointPos;
    g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(user, joint, jointPos);
    XnPoint3D pt = jointPos.position;
    
//    printf("%f %f %f confidence %f\n",jointPos.position.X, jointPos.position.Y, jointPos.position.Z, jointPos.fConfidence);

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
    if (!PointIsValid(p1) || !PointIsValid(p2)) return;
    
    float dx = p1.X-p2.X;
    float dy = p1.Y-p2.Y;
    float l = sqrt(dx*dx+dy*dy);
    dx /= l;
    dy /= l;
    
//    printf("p1 (%f, %f) (%f, %f) (%f, %f) p2 (%f, %f) (%f, %f) (%f, %f)\n", p1.X+(w/2)*dy, p1.Y-(w/2)*dx, p1.X, p1.Y, p1.X-(w/2)*dy, p1.Y+(w/2)*dx,
//    p2.X+(w/2)*dy, p2.Y-(w/2)*dx, p2.X, p2.Y, p2.X-(w/2)*dy, p2.Y+(w/2)*dx);
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
    if (!PointIsValid(p)) return;
    
    int s = 2;
    
    cv::Point2f cameraPoints[] = {cv::Point2f(p.X-s, p.Y-s), cv::Point2f(p.X+s, p.Y-s), cv::Point2f(p.X-s, p.Y+s), cv::Point2f(p.X+s, p.Y+s)};
    cv::Point2f skinPoints[] = {cv::Point2f(0, 0), cv::Point2f(3, 0), cv::Point2f(0, 3), cv::Point2f(3, 3)};
    
    cv::Size size = cv::Size(4, 4);
    cv::Mat transform = cv::getPerspectiveTransform(cameraPoints, skinPoints);
    cv::Mat transformed = cv::Mat(size, CV_8UC3);
    cv::warpPerspective(*body, transformed, transform, size);
    
    CopyBodyPart(&transformed, skin, pos);
}

void CleanFace(cv::Mat *face)
{
    // Add some eyes
    unsigned char *row = face->ptr<unsigned char>(3);
    row += 3;
    *row++ = 200;
    *row++ = 200;
    *row++ = 200;
    *row++ = 0x02;
    *row++ = 0x08;
    *row++ = 0x10;
    row += 6;
    *row++ = 0x02;
    *row++ = 0x08;
    *row++ = 0x10;
    *row++ = 200;
    *row++ = 200;
    *row++ = 200;
}

void GetHead(XnUserID user, cv::Mat *body, cv::Mat *skin)
{
    XnPoint3D h = PointForJoint(user, XN_SKEL_HEAD);
    if (!PointIsValid(h)) return;
    
    int w = 12;
    cv::Point2f tl = cv::Point2f(h.X+w, h.Y-w*2.0);
    cv::Point2f tr = cv::Point2f(h.X-w, h.Y-w*2.0);
    cv::Point2f bl = cv::Point2f(h.X+w*0.75, h.Y+w*1.5);
    cv::Point2f br = cv::Point2f(h.X-w*0.75, h.Y+w*1.5);
    
    cv::Point2f xoffset = cv::Point2f(1.0, 0.0);
    cv::Point2f yoffset = cv::Point2f(0.0, 1.0);
    
    cv::Point2f facePoints[] = {tl, tr, bl, br};
    cv::Point2f leftPoints[] = {tl+xoffset*4.0, tl, bl+xoffset*4.0, bl};
    cv::Point2f rightPoints[] = {tr, tr-xoffset*4.0, br, br-xoffset*4.0};
    cv::Point2f topPoints[] = {tl-yoffset*4.0, tr-yoffset*4.0, tl, tr};
    cv::Point2f bottomPoints[] = {bl, br, bl+yoffset*4.0, br+yoffset*4.0};
    
    cv::Point2f skinPoints[] = {cv::Point2f(7, 0), cv::Point2f(0, 0), cv::Point2f(7, 7), cv::Point2f(0, 7)};
    cv::Size size = cv::Size(8, 8);
    
    cv::Mat transform = cv::getPerspectiveTransform(facePoints, skinPoints);
    cv::Mat transformed = cv::Mat(size, CV_8UC3);
    cv::warpPerspective(*body, transformed, transform, size);
    CleanFace(&transformed);
    CopyBodyPart(&transformed, skin, cv::Point2i(8, 8));
    
    transform = cv::getPerspectiveTransform(leftPoints, skinPoints);
    transformed = cv::Mat(size, CV_8UC3);
    cv::warpPerspective(*body, transformed, transform, size);
    CopyBodyPart(&transformed, skin, cv::Point2i(16, 8));
    
    transform = cv::getPerspectiveTransform(rightPoints, skinPoints);
    transformed = cv::Mat(size, CV_8UC3);
    cv::warpPerspective(*body, transformed, transform, size);
    CopyBodyPart(&transformed, skin, cv::Point2i(0, 8));
    
    transform = cv::getPerspectiveTransform(topPoints, skinPoints);
    transformed = cv::Mat(size, CV_8UC3);
    cv::warpPerspective(*body, transformed, transform, size);
    CopyBodyPart(&transformed, skin, cv::Point2i(8, 0));
    
    // Use the forehead/top area as the back as well
    transform = cv::getPerspectiveTransform(topPoints, skinPoints);
    transformed = cv::Mat(size, CV_8UC3);
    cv::warpPerspective(*body, transformed, transform, size);
    CopyBodyPart(&transformed, skin, cv::Point2i(24, 8));
    
    transform = cv::getPerspectiveTransform(bottomPoints, skinPoints);
    transformed = cv::Mat(size, CV_8UC3);
    cv::warpPerspective(*body, transformed, transform, size);
    CopyBodyPart(&transformed, skin, cv::Point2i(16, 0));
}

void GetTorso(XnUserID user, cv::Mat *body, cv::Mat *skin)
{
    XnPoint3D ls = PointForJoint(user, XN_SKEL_LEFT_SHOULDER);
    XnPoint3D rs = PointForJoint(user, XN_SKEL_RIGHT_SHOULDER);
    XnPoint3D lh = PointForJoint(user, XN_SKEL_LEFT_HIP);
    XnPoint3D rh = PointForJoint(user, XN_SKEL_RIGHT_HIP);
    if (!PointIsValid(ls) || !PointIsValid(rs) || !PointIsValid(lh) || !PointIsValid(rh)) return;
    
//    printf("(%f,%f,%f) (%f, %f, %f) (%f, %f, %f) (%f, %f, %f)\n", ls.X, ls.Y, ls.Z, rs.X, rs.Y, rs.Z, lh.X, lh.Y, lh.Z, rh.X, rh.Y, rh.Z);
    
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
    // Head
    GetHead(user, body, skin);

    // Torso and sides
    GetTorso(user, body, skin);
    GetLimb(user, body, skin, XN_SKEL_RIGHT_SHOULDER, XN_SKEL_RIGHT_HIP, 6, cv::Size(4,12), cv::Point2i(16,20));
    GetLimb(user, body, skin, XN_SKEL_LEFT_SHOULDER, XN_SKEL_LEFT_HIP, 6, cv::Size(4,12), cv::Point2i(28,20));
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

void DrawJointPoint(XnUserID user, cv::Mat *input, XnSkeletonJoint joint)
{
    XnPoint3D p = PointForJoint(user, joint);
    if (!PointIsValid(p)) return;
    cv::Point2i point = cv::Point2i(p.X, p.Y);

    for(int y = point.y-1; y < point.y+2; y++) {
        unsigned char *row = input->ptr<unsigned char>(y);
        row+= (point.x-1)*3;
        *row++ = 0;
        *row++ = 0;
        *row++ = 255;
        *row++ = 0;
        *row++ = 0;
        *row++ = 255;
        *row++ = 0;
        *row++ = 0;
        *row++ = 255;
    }
}

void DrawDebugPoints(XnUserID user, cv::Mat *input)
{
    DrawJointPoint(user, input, XN_SKEL_HEAD);
    DrawJointPoint(user, input, XN_SKEL_NECK);
    DrawJointPoint(user, input, XN_SKEL_RIGHT_SHOULDER);
    DrawJointPoint(user, input, XN_SKEL_RIGHT_ELBOW);
    DrawJointPoint(user, input, XN_SKEL_RIGHT_HAND);
    DrawJointPoint(user, input, XN_SKEL_LEFT_SHOULDER);
    DrawJointPoint(user, input, XN_SKEL_LEFT_ELBOW);
    DrawJointPoint(user, input, XN_SKEL_LEFT_HAND);
}

void SegmentUser(XnUserID user, cv::Mat *input, const xn::SceneMetaData& smd)
{
    const XnLabel* pLabels = smd.Data();

    for(int y = 0; y < input->rows; y++) {
        unsigned char *row = input->ptr<unsigned char>(y);
        for (int x = 0; x < input->cols; x++) {
            XnLabel label = *pLabels++;
            if (label != user) {
                *row++ = 0;
                *row++ = 0;
                *row++ = 0;
            } else {
                row+=3;
            }
        }
    }
}

void GenerateMinecraftCharacter(const xn::DepthMetaData& dmd, const xn::SceneMetaData& smd, const XnRGB24Pixel* image)
{
    int xRes = dmd.XRes();
    int yRes = dmd.YRes();
    
    cv::Mat inputImage = cv::Mat(yRes, xRes, CV_8UC3);
    cv::Mat skin = cv::Mat::zeros(cv::Size(64,32), CV_8UC3);
    XnToCV(image,&inputImage);
    cv::cvtColor(inputImage,inputImage,CV_RGB2BGR);
    
    
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
	SegmentUser(aUsers[i], &inputImage, smd);
	DrawDebugPoints(aUsers[i], &inputImage);
	cv::imwrite("blah.png",inputImage);
	sync();
	system("convert skin.png -transparent black skin.png && composite -geometry +32+0 hardhat.png skin.png skin.png");
}