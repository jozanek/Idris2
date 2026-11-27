import System
import System.Concurrency

meet : (own, other : String) -> IO Int
meet own other = System.system "sh meet.sh \{own} \{other}"

-- Each command waits for the other one. So both can only succeed when two
-- threads can run commands with `system` at the same time.
main : IO ()
main = do
  firstResult <- makeChannel
  thread <- fork $ do status <- meet "first" "second"
                      channelPut firstResult status
  second <- meet "second" "first"
  first <- channelGet firstResult
  threadWait thread
  putStrLn "first command: \{show first}"
  putStrLn "second command: \{show second}"
