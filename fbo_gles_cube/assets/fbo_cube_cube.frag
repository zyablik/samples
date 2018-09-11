/*
 * This proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009 - 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

precision mediump float;
varying vec2 v_texCoord;
uniform sampler2D s_texture;
void main()
{
  gl_FragColor = texture2D(s_texture, v_texCoord);
//  gl_FragColor = vec4 (0.0, 1.0, 0.0, 1.0);
}
