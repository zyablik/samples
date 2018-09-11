/*
 * This proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009 - 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

attribute vec4 av4position;
attribute vec2 a_texCoord;
varying vec2 v_texCoord;

uniform mat4 mvp;

void main() {
	gl_Position = mvp * av4position;
  v_texCoord = a_texCoord;
}
