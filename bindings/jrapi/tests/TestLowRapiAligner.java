
import it.crs4.rapi.*;
import it.crs4.rapi.RapiUtils;
import it.crs4.rapi.AlignOp;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.List;

import org.junit.*;
import static org.junit.Assert.*;

public class TestLowRapiAligner
{
  private static File jrapiBaseDir;

  private Opts rapiOpts;
  private Ref refObj;
  private Batch reads;
  private AlignerState aligner;
  private File miniRefPath;

  @BeforeClass
  public static void initSharedObj()
  {
    RapiUtils.loadPlugin(null);
  }

  @Before
  public void init() throws RapiException, IOException
  {
    rapiOpts = new Opts();
    Rapi.init(rapiOpts);

    reads = new Batch(2);
    TestUtils.appendSeqsToBatch(TestUtils.readMiniRefSeqs(), reads);

    // load the reference
    miniRefPath = new File(jrapiBaseDir, TestUtils.RELATIVE_MINI_REF);
    refObj = new Ref(miniRefPath.getAbsolutePath());

    aligner = new AlignerState(rapiOpts);
    aligner.alignReads(refObj, reads);
  }

  @After
  public void tearDown() throws RapiException
  {
    aligner = null;
    refObj.unload();
    refObj = null;
    if (reads != null)
      reads.clear();
    reads = null;
    Rapi.shutdown();
  }

  @Test
  public void testReadAttributes() throws RapiException
  {
    Read rapiRead = reads.getRead(0, 0);
    assertEquals("read_00", rapiRead.getId());
    // shortcut attributes that access the first Alignment
    assertFalse(rapiRead.getPropPaired());
    assertTrue(rapiRead.getMapped());
    assertFalse(rapiRead.getReverseStrand());
    assertEquals(60, rapiRead.getMapq());
    assertEquals(60, rapiRead.getScore());
  }

  @Test
  public void testAlignmentStruct() throws RapiException
  {
    Read rapiRead = reads.getRead(0, 0);
    assertEquals("read_00", rapiRead.getId());
    assertTrue(rapiRead.getNAlignments() > 0);

    Alignment aln = rapiRead.getAln(0);
    assertEquals("chr1", aln.getContig().getName());
    assertEquals(32461, aln.getPos());
    assertEquals("60M", aln.getCigarString());

    AlignOp[] ops = aln.getCigarOps();
    assertEquals(1, ops.length);
    assertEquals(AlignOp.Type.Match, ops[0].getType());
    assertEquals(60, ops[0].getLen());

    assertEquals(60, aln.getMapq());
    assertEquals(60, aln.getScore());

    // Flag 65 is 'p1'
    assertTrue(aln.getPaired());
    assertFalse(aln.getPropPaired());
    assertTrue(aln.getMapped());
    assertFalse(aln.getReverseStrand());
    assertFalse(aln.getSecondaryAln());
    assertEquals(0, aln.getNMismatches());
    assertEquals(0, aln.getNGapOpens());
    assertEquals(0, aln.getNGapExtensions());

    /* Tags not yet implememnted
    expected_tags = dict(
        MD='60',
        XS=0)
# the SAM has more tags (NM and AS), but RAPI makes that information
# available through other members of the Alignment structure.
      self.assertEqual(expected_tags, aln.get_tags())
      */

    // check out reads that don't align perfectly
    aln = reads.getRead(1, 0).getAln(0);
    assertEquals(60, aln.getMapq());
    assertEquals(51, aln.getScore());
    assertEquals("11M3D49M", aln.getCigarString());

    assertArrayEquals(
        new AlignOp[] { new AlignOp('M', 11), new AlignOp('D', 3), new AlignOp('M', 49) },
        aln.getCigarOps());

    assertEquals(3, aln.getNMismatches());
    /* XXX: tags not implemented yet
    md_tag = aln.get_tags()['MD']
    assertEquals('11^CCC49', md_tag)
    */
    assertEquals(0, aln.getNGapOpens());
    assertEquals(0, aln.getNGapExtensions());

    aln = reads.getRead(2, 0).getAln(0);
    assertEquals(60, aln.getMapq());
    assertEquals(48, aln.getScore());
    assertEquals("13M3I44M", aln.getCigarString());

    assertArrayEquals(
        new AlignOp[] { new AlignOp('M', 13), new AlignOp('I', 3), new AlignOp('M', 44) },
        aln.getCigarOps());
    assertEquals(3, aln.getNMismatches());

    /* XXX: tags not implemented yet
      md_tag = aln.get_tags()['MD']
      self.assertEqual('57', md_tag)
      */

    aln = reads.getRead(3, 0).getAln(0);
    assertEquals(60, aln.getMapq());
    assertEquals(50, aln.getScore());
    assertEquals("60M", aln.getCigarString());
    assertEquals(2, aln.getNMismatches());
    /* XXX: tags not implemented yet
      md_tag = aln.get_tags()['MD'];
      assertEquals('15T16C27', md_tag);
      */
  }

  public static void main(String args[])
  {
    TestUtils.testCaseMainMethod(TestLowRapiAligner.class.getName(), args);
  }
}

// vim: set et sw=2
