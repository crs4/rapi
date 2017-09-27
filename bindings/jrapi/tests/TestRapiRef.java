/************************************************************************************
 * This code is published under the The MIT License.
 *
 * Copyright (c) 2016 Center for Advanced Studies,
 *                      Research and Development in Sardinia (CRS4), Pula, Italy.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ************************************************************************************/



import it.crs4.rapi.*;
import it.crs4.rapi.RapiUtils;

import java.util.Iterator;

import org.junit.*;
import static org.junit.Assert.*;

public class TestRapiRef
{
  private Opts optsObj;
  private Ref refObj;

  @BeforeClass
  public static void initSharedObj()
  {
    RapiUtils.loadPlugin();
  }

  @Before
  public void init() throws RapiException
  {
    optsObj = new Opts();
    optsObj.setShareRefMem(false);
    Rapi.init(optsObj);
    refObj = new Ref();
    refObj.load(TestUtils.RELATIVE_MINI_REF);
  }

  @After
  public void tearDown() throws RapiException
  {
    refObj.unload();
    Rapi.shutdown();
  }

  @Test
  public void testGetters()
  {
    assertEquals(1, refObj.getNContigs());
    assertTrue(refObj.getPath().length() > 1);
  }

  @Test
  public void testContig()
  {
    Contig c = refObj.getContig(0);
    assertNotNull(c);

    assertEquals("chr1", c.getName());
    assertEquals(60000, c.getLen());
    assertNull(c.getAssemblyIdentifier());
    assertNull(c.getSpecies());
    assertNull(c.getSpecies());
    assertNull(c.getUri());
    assertNull(c.getMd5());
  }

  @Test(expected=RapiInvalidParamException.class)
  public void testGetContigOOB1()
  {
    refObj.getContig(1);
  }

  @Test(expected=RapiInvalidParamException.class)
  public void testGetContigOOB2()
  {
    refObj.getContig(-1);
  }

  @Test
  public void testContigIterator()
  {
    Iterator<Contig> it = refObj.iterator();
    assertTrue(it.hasNext());

    Contig c = it.next();
    assertFalse(it.hasNext());
    assertNotNull(c);

    // verify that we fetched our contig
    assertEquals("chr1", c.getName());
    assertEquals(60000, c.getLen());
  }

  @Test
  public void testDoubleUnload()
  {
    // double unload shouldn't cause problems
    refObj.unload();
    refObj.unload();
  }

  @Test
  public void testFormatSAMHeader() throws RapiException
  {
    String hdr = Rapi.formatSamHdr(refObj);
    assertTrue(hdr.startsWith("@SQ"));
  }

  @Test(expected=RapiException.class)
  public void testFormatSAMHeaderError() throws RapiException
  {
    String hdr = Rapi.formatSamHdr(null);
  }

  public static void main(String args[])
  {
    TestUtils.testCaseMainMethod(TestRapiRef.class.getName(), args);
  }
}

// vim: set et sw=2
