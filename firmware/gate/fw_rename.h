/* Gate builds: the firmware's MARS_* become fw_MARS_*, and user mode gets MARS_* as
   ecall stubs (gate_user.c).  Pre-included into mars.c and gate_fw.c. */
#define MARS_SelfTest fw_MARS_SelfTest
#define MARS_CapabilityGet fw_MARS_CapabilityGet
#define MARS_SequenceHash fw_MARS_SequenceHash
#define MARS_SequenceUpdate fw_MARS_SequenceUpdate
#define MARS_SequenceComplete fw_MARS_SequenceComplete
#define MARS_PcrExtend fw_MARS_PcrExtend
#define MARS_RegRead fw_MARS_RegRead
#define MARS_Derive fw_MARS_Derive
#define MARS_DpDerive fw_MARS_DpDerive
#define MARS_PublicRead fw_MARS_PublicRead
#define MARS_Quote fw_MARS_Quote
#define MARS_Sign fw_MARS_Sign
#define MARS_SignatureVerify fw_MARS_SignatureVerify
