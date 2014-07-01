/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2014 Crytek
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/


#include "driver/gl/gl_manager.h"
#include "driver/gl/gl_driver.h"

bool GLResourceManager::SerialisableResource(ResourceId id, GLResourceRecord *record)
{
	if(id == m_GL->GetContextResourceID())
		return false;
	return true;
}

bool GLResourceManager::Prepare_InitialState(GLResource res)
{
	if(res.Namespace == eResBuffer)
	{
		GLResourceRecord *record = GetResourceRecord(res);
		
		GLint length;
		m_GL->glGetNamedBufferParameterivEXT(res.name, eGL_BUFFER_SIZE, &length);
	
		m_GL->glGetNamedBufferSubDataEXT(res.name, 0, length, record->GetDataPtr());
	}

	return true;
}
