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



package it.crs4.rapi;

import java.util.Iterator;

public class ReadAlnIterator implements Iterator<Alignment> {
  private Read read;
  private int nextAln;
  private int nAlns;

  public ReadAlnIterator(Read r)
  {
    if (r == null)
      throw new NullPointerException();
    read = r;
    nextAln = 0;
    nAlns = read.getNAlignments();
  }

  public Alignment next()
  {
    if (nextAln >= nAlns)
      throw new java.util.NoSuchElementException();

    Alignment a = read.getAln(nextAln);
    nextAln += 1;
    return a;
  }

  public boolean hasNext()
  {
    return nextAln < nAlns;
  }

  public void remove()
  {
    throw new UnsupportedOperationException();
  }
}