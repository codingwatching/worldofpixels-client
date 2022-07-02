#pragma once

// camera should be the center of the viewport
class Camera {
	float x;
	float y;
	float zoom;

public:
	Camera();
	virtual ~Camera();

	float getX() const;
	float getY() const;
	float getZoom() const;
	void getWorldPosFromScreenPos(float sx, float sy, float * wx, float * wy) const;

	virtual void getScreenSize(int * w, int * h) const = 0;
	virtual void setPos(float x, float y);
	virtual void setZoom(float z);
	virtual void setZoom(float z, float ox, float oy); // with non-center camera-coords origin
	
	virtual void translate(float dx, float dy);

private:
	virtual void recalculateCursorPosition() const = 0;
};
